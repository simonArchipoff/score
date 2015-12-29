#pragma once
#include <Loop/LoopViewUpdater.hpp>
#include <Loop/Palette/LoopToolPalette.hpp>
#include <Process/LayerPresenter.hpp>
#include <Scenario/Document/BaseScenario/BaseScenarioPresenter.hpp>
#include <QDebug>
#include <QPoint>

#include <Process/Focus/FocusDispatcher.hpp>
#include <Process/ProcessContext.hpp>
#include <Process/ZoomHelper.hpp>
#include <iscore/tools/SettableIdentifier.hpp>

namespace Process { class LayerModel; }
namespace Process { class ProcessModel; }
class QMenu;
class QObject;
class TemporalConstraintPresenter;
namespace Loop {
class ProcessModel;
}  // namespace Loop
namespace iscore {
class CommandStackFacade;
struct DocumentContext;
}  // namespace iscore


namespace Loop
{
inline void removeSelection(const Loop::ProcessModel& model, iscore::CommandStackFacade& )
{

}

inline void clearContentFromSelection(const Loop::ProcessModel& model, iscore::CommandStackFacade& )
{
    ISCORE_TODO;
}
}

class LoopLayer;
class LoopView;

class LoopPresenter :
        public Process::LayerPresenter,
        public BaseScenarioPresenter<Loop::ProcessModel, TemporalConstraintPresenter>
{
        Q_OBJECT
        friend class LoopViewUpdater;
    public:
        LoopPresenter(
                const iscore::DocumentContext& context,
                const LoopLayer&,
                LoopView* view,
                QObject* parent);

        ~LoopPresenter();
        LoopView& view() const
        { return *m_view; }

        using BaseScenarioPresenter<Loop::ProcessModel, TemporalConstraintPresenter>::event;
        using QObject::event;

        void setWidth(qreal width) override;
        void setHeight(qreal height) override;

        void putToFront() override;
        void putBehind() override;

        void on_zoomRatioChanged(ZoomRatio) override;
        void parentGeometryChanged() override;

        const Process::LayerModel& layerModel() const override;
        const Id<Process::ProcessModel>& modelId() const override;

        ZoomRatio zoomRatio() const
        { return m_zoomRatio; }

        void fillContextMenu(QMenu*, const QPoint& pos, const QPointF& scenepos) const override;

    signals:
        void pressed(QPointF);
        void moved(QPointF);
        void released(QPointF);
        void escPressed();

    private:
        const LoopLayer& m_layer;
        LoopView* m_view{};

        ZoomRatio m_zoomRatio {};

        LoopViewUpdater m_viewUpdater;

        FocusDispatcher m_focusDispatcher;
        LayerContext m_context;
        LoopToolPalette m_palette;
        void updateAllElements();
};
