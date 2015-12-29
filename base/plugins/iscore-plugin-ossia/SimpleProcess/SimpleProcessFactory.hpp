#pragma once

#include <Process/ProcessFactory.hpp>
#include "SimpleProcessModel.hpp"
#include <DummyProcess/DummyLayerPresenter.hpp>
#include <DummyProcess/DummyLayerView.hpp>
#include <iscore/serialization/VisitorCommon.hpp>


class SimpleProcessFactory : public Process::ProcessFactory
{
    public:
        QString prettyName() const override
        { return QObject::tr("SimpleProcess"); }

        const ProcessFactoryKey& key_impl() const override
        {
            static const ProcessFactoryKey name{"SimpleProcessModel"};
            return name;
        }

        Process::ProcessModel* makeModel(
                const TimeValue& duration,
                const Id<Process::ProcessModel>& id,
                QObject* parent) override
        {
            return new SimpleProcessModel{duration, id, parent};
        }

        QByteArray makeStaticLayerConstructionData() const override
        {
            return {};
        }

        Process::ProcessModel* loadModel(const VisitorVariant& vis, QObject* parent) override
        {
            return deserialize_dyn(vis, [&] (auto&& deserializer)
            { return new SimpleProcessModel{deserializer, parent};});
        }

        Process::LayerPresenter* makeLayerPresenter(
                const Process::LayerModel& model,
                Process::LayerView* v,
                QObject* parent) override
        {
            return new DummyLayerPresenter{model, dynamic_cast<DummyLayerView*>(v), parent};
        }

        Process::LayerView* makeLayerView(
                const Process::LayerModel&,
                QGraphicsItem* parent) override
        {
            return new DummyLayerView{parent};
        }
};
