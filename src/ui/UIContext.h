#pragma once

#include "UIBuilder.h"
#include "../components/Button.h"
#include "../components/ComboBox.h"
#include "../components/Image.h"
#include "../components/InputBox.h"
#include "../components/Label.h"
#include "../components/Panel.h"
#include "../components/Polygon.h"
#include "../components/ProgressBar.h"
#include "../components/ScrollArea.h"
#include "../components/SegmentedControl.h"
#include "../components/Sidebar.h"
#include "../components/Slider.h"
#include <memory>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace EUINEO {

enum class FlexDirection {
    Row,
    Column
};

class UIContext {
private:
    struct LayoutState;

public:

    class LayoutBuilder {
    public:
        LayoutBuilder(UIContext& context, FlexDirection direction);

        LayoutBuilder& direction(FlexDirection value);
        LayoutBuilder& x(float value);
        LayoutBuilder& y(float value);
        LayoutBuilder& position(float xValue, float yValue);
        LayoutBuilder& width(float value);
        LayoutBuilder& height(float value);
        LayoutBuilder& size(float widthValue, float heightValue);
        LayoutBuilder& flex(float value);
        LayoutBuilder& margin(float value);
        LayoutBuilder& margin(float horizontal, float vertical);
        LayoutBuilder& margin(float left, float top, float right, float bottom);
        LayoutBuilder& gap(float value);
        LayoutBuilder& padding(float value);
        LayoutBuilder& padding(float horizontal, float vertical);
        LayoutBuilder& padding(float left, float top, float right, float bottom);

        template <typename Fn>
        void content(Fn&& compose) {
            context_.beginLayout(layout_);
            std::forward<Fn>(compose)();
            context_.endLayout(layout_);
        }

    private:
        UIContext& context_;
        LayoutState* layout_ = nullptr;
    };

    void begin(const std::string& pageId);
    void end();
    void update();
    void render();
    void draw();
    bool wantsContinuousUpdate() const;
    void markAllNodesDirty();
    void requestVisualRefresh(float duration = 0.0f);
    void requestThemeRefresh(float duration = 0.18f);
    void pushClip(float x, float y, float width, float height);
    void popClip();
    float pushScrollArea(const std::string& id, float x, float y, float width, float height,
                         float contentHeight, float scrollStep = 48.0f,
                         RenderLayer scrollbarLayer = RenderLayer::Content);
    void popScrollArea();
    bool consumeRecomposeRequest();

    LayoutBuilder row() {
        return LayoutBuilder(*this, FlexDirection::Row);
    }

    LayoutBuilder column() {
        return LayoutBuilder(*this, FlexDirection::Column);
    }

    LayoutBuilder flex() {
        return LayoutBuilder(*this, FlexDirection::Row);
    }

    template <typename NodeT>
    GenericNodeBuilder<NodeT> node(const std::string& id) {
        NodeT& instance = acquire<NodeT>(id);
        return GenericNodeBuilder<NodeT>(*this, instance);
    }

    template <typename NodeT>
    typename NodeT::Builder component(const std::string& id) {
        NodeT& node = acquire<NodeT>(id);
        return typename NodeT::Builder(*this, node);
    }

#define EUI_UI_COMPONENT(name, type) \
    typename type::Builder name(const std::string& id) { \
        return component<type>(id); \
    }
#include "UIComponents.def"
#undef EUI_UI_COMPONENT

    template <typename Fn>
    float scrollArea(const std::string& id, float x, float y, float width, float height,
                     float contentHeight, Fn&& compose, float scrollStep = 48.0f) {
        const float scrollOffsetY = pushScrollArea(id, x, y, width, height, contentHeight, scrollStep);
        if constexpr (std::is_invocable_v<Fn, float>) {
            std::forward<Fn>(compose)(scrollOffsetY);
        } else {
            std::forward<Fn>(compose)();
        }
        popScrollArea();
        return scrollOffsetY;
    }

    template <typename Fn>
    void popup(const std::string& id, float x, float y, float width, float height, Fn&& compose) {
        popupPanel(id)
            .position(x, y)
            .size(width, height)
            .build();
        pushClip(x, y, width, height);
        if constexpr (std::is_invocable_v<Fn, const RectFrame&>) {
            std::forward<Fn>(compose)(RectFrame{x, y, width, height});
        } else {
            std::forward<Fn>(compose)();
        }
        popClip();
    }

private:
    struct Offset {
        float x = 0.0f;
        float y = 0.0f;
    };

    struct LayoutItem {
        UINode* node = nullptr;
        struct LayoutState* container = nullptr;
        LayoutBuildInfo build;
    };

    struct LayoutState {
        FlexDirection direction = FlexDirection::Row;
        LayoutBuildInfo build;
        float gap = 0.0f;
        float paddingLeft = 0.0f;
        float paddingTop = 0.0f;
        float paddingRight = 0.0f;
        float paddingBottom = 0.0f;
        std::vector<LayoutItem> children;
    };

    template <typename NodeT>
    NodeT& acquire(const std::string& id) {
        const std::string fullKey = pageId_.empty() ? id : pageId_ + "." + id;
        auto it = nodes_.find(fullKey);
        if (it == nodes_.end() || std::string(it->second->typeName()) != NodeT::StaticTypeName()) {
            treeChanged_ = true;
            auto replacement = std::make_unique<NodeT>(fullKey);
            UINode* raw = replacement.get();
            it = nodes_.insert_or_assign(fullKey, std::move(replacement)).first;
            raw->beginCompose(composeStamp_);
            applyCurrentContext(raw);
            order_.push_back(raw);
            return static_cast<NodeT&>(*raw);
        }

        UINode* node = it->second.get();
        if (!node->composedIn(composeStamp_)) {
            node->beginCompose(composeStamp_);
            order_.push_back(node);
        }
        applyCurrentContext(node);
        return static_cast<NodeT&>(*node);
    }

    void applyCurrentContext(UINode* node) {
        if (node == nullptr) {
            return;
        }
        UIPrimitive& primitive = node->primitive();
        primitive.contextOffsetX = currentOffsetX_;
        primitive.contextOffsetY = currentOffsetY_;
        baseContextOffset_[node] = Offset{currentOffsetX_, currentOffsetY_};
        scrollBindings_[node] = scrollScopeStack_;
        if (clipStack_.empty() || !primitive.clipToParent) {
            primitive.hasClipRect = false;
            primitive.clipRect = UIClipRect{};
            return;
        }
        primitive.hasClipRect = true;
        primitive.clipRect = clipStack_.back();
    }

    void pushOffset(float x, float y) {
        offsetStack_.push_back(Offset{x, y});
        currentOffsetX_ += x;
        currentOffsetY_ += y;
    }

    void popOffset() {
        if (offsetStack_.empty()) {
            return;
        }
        currentOffsetX_ -= offsetStack_.back().x;
        currentOffsetY_ -= offsetStack_.back().y;
        offsetStack_.pop_back();
    }
    void applyRuntimeContext(UINode* node);

    LayoutState* createLayout(FlexDirection direction);
    void beginLayout(LayoutState* layout);
    void endLayout(LayoutState* layout);
    void finalizeBuild(UINode& node, const LayoutBuildInfo& info);
    void registerLayoutChild(UINode& node, const LayoutBuildInfo& info);
    void resolveLayout(LayoutState& layout, const RectFrame& frame);
    RectFrame resolveLayoutFrame(const LayoutState& layout) const;
    float preferredMainSize(const LayoutItem& item, FlexDirection direction) const;
    float resolveMainSize(const LayoutItem& item, FlexDirection direction, float flexSpace, float totalFlex) const;
    float resolveCrossSize(const LayoutItem& item, FlexDirection direction, float availableCross) const;
    RectFrame resolveItemFrame(const LayoutItem& item, FlexDirection direction,
                              float cursor, float crossStart, float mainSize, float crossSize) const;
    void applyResolvedFrame(UINode& node, const RectFrame& frame);

    friend void FinalizeUIBuild(UIContext& context, UINode& node, const LayoutBuildInfo& info);

    std::string pageId_;
    std::uint64_t composeStamp_ = 0;
    std::unordered_map<std::string, std::unique_ptr<UINode>> nodes_;
    std::vector<UINode*> order_;
    std::vector<UINode*> drawOrder_;
    std::vector<UIClipRect> clipStack_;
    std::vector<Offset> offsetStack_;
    std::vector<ScrollAreaNode*> scrollScopeStack_;
    std::vector<std::unique_ptr<LayoutState>> ownedLayouts_;
    std::vector<LayoutState*> layoutStack_;
    std::unordered_map<UINode*, Offset> baseContextOffset_;
    std::unordered_map<UINode*, std::vector<ScrollAreaNode*>> scrollBindings_;
    bool treeChanged_ = false;
    bool needsRecompose_ = false;
    std::uint64_t drawOrderStamp_ = 0;
    float currentOffsetX_ = 0.0f;
    float currentOffsetY_ = 0.0f;
};

inline UIContext::LayoutBuilder::LayoutBuilder(UIContext& context, FlexDirection direction)
    : context_(context), layout_(context.createLayout(direction)) {}

inline UIContext::LayoutBuilder& UIContext::LayoutBuilder::direction(FlexDirection value) {
    layout_->direction = value;
    return *this;
}

inline UIContext::LayoutBuilder& UIContext::LayoutBuilder::x(float value) {
    layout_->build.hasX = true;
    layout_->build.x = value;
    return *this;
}

inline UIContext::LayoutBuilder& UIContext::LayoutBuilder::y(float value) {
    layout_->build.hasY = true;
    layout_->build.y = value;
    return *this;
}

inline UIContext::LayoutBuilder& UIContext::LayoutBuilder::position(float xValue, float yValue) {
    layout_->build.hasX = true;
    layout_->build.hasY = true;
    layout_->build.x = xValue;
    layout_->build.y = yValue;
    return *this;
}

inline UIContext::LayoutBuilder& UIContext::LayoutBuilder::width(float value) {
    layout_->build.hasWidth = true;
    layout_->build.width = value;
    return *this;
}

inline UIContext::LayoutBuilder& UIContext::LayoutBuilder::height(float value) {
    layout_->build.hasHeight = true;
    layout_->build.height = value;
    return *this;
}

inline UIContext::LayoutBuilder& UIContext::LayoutBuilder::size(float widthValue, float heightValue) {
    layout_->build.hasWidth = true;
    layout_->build.hasHeight = true;
    layout_->build.width = widthValue;
    layout_->build.height = heightValue;
    return *this;
}

inline UIContext::LayoutBuilder& UIContext::LayoutBuilder::flex(float value) {
    layout_->build.flex = std::max(0.0f, value);
    return *this;
}

inline UIContext::LayoutBuilder& UIContext::LayoutBuilder::margin(float value) {
    const float clamped = std::max(0.0f, value);
    layout_->build.marginLeft = clamped;
    layout_->build.marginTop = clamped;
    layout_->build.marginRight = clamped;
    layout_->build.marginBottom = clamped;
    return *this;
}

inline UIContext::LayoutBuilder& UIContext::LayoutBuilder::margin(float horizontal, float vertical) {
    layout_->build.marginLeft = std::max(0.0f, horizontal);
    layout_->build.marginRight = std::max(0.0f, horizontal);
    layout_->build.marginTop = std::max(0.0f, vertical);
    layout_->build.marginBottom = std::max(0.0f, vertical);
    return *this;
}

inline UIContext::LayoutBuilder& UIContext::LayoutBuilder::margin(float left, float top, float right, float bottom) {
    layout_->build.marginLeft = std::max(0.0f, left);
    layout_->build.marginTop = std::max(0.0f, top);
    layout_->build.marginRight = std::max(0.0f, right);
    layout_->build.marginBottom = std::max(0.0f, bottom);
    return *this;
}

inline UIContext::LayoutBuilder& UIContext::LayoutBuilder::gap(float value) {
    layout_->gap = std::max(0.0f, value);
    return *this;
}

inline UIContext::LayoutBuilder& UIContext::LayoutBuilder::padding(float value) {
    const float clamped = std::max(0.0f, value);
    layout_->paddingLeft = clamped;
    layout_->paddingTop = clamped;
    layout_->paddingRight = clamped;
    layout_->paddingBottom = clamped;
    return *this;
}

inline UIContext::LayoutBuilder& UIContext::LayoutBuilder::padding(float horizontal, float vertical) {
    layout_->paddingLeft = std::max(0.0f, horizontal);
    layout_->paddingRight = std::max(0.0f, horizontal);
    layout_->paddingTop = std::max(0.0f, vertical);
    layout_->paddingBottom = std::max(0.0f, vertical);
    return *this;
}

inline UIContext::LayoutBuilder& UIContext::LayoutBuilder::padding(float left, float top, float right, float bottom) {
    layout_->paddingLeft = std::max(0.0f, left);
    layout_->paddingTop = std::max(0.0f, top);
    layout_->paddingRight = std::max(0.0f, right);
    layout_->paddingBottom = std::max(0.0f, bottom);
    return *this;
}

} // namespace EUINEO
