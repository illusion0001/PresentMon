#pragma once
#include "../../PresentMonAPI2/source/PresentMonAPI.h"
#include <iterator>
#include <string>
#include <ranges>
#include <memory>
#include <unordered_map>
#include <format>
#include <functional>
#include <cassert>
#include "Exception.h"

namespace pmapi
{
    namespace intro
    {
        bool MetricTypeIsDynamic(PM_METRIC_TYPE type);
        bool MetricTypeIsFrameEvent(PM_METRIC_TYPE type);

        template<class T>
        class ViewIterator
        {
        public:
            using iterator_category = std::random_access_iterator_tag;
            using value_type = T;
            using base_type = typename T::BaseType;
            using reference = value_type&;
            using pointer = value_type*;
            using difference_type = ptrdiff_t;

            ViewIterator() = default;
            ViewIterator(const class Root* pRoot_, const PM_INTROSPECTION_OBJARRAY* pObjArray, difference_type offset = 0u) noexcept
                :
                pRoot{ pRoot_ },
                pArray{ (const base_type* const*)pObjArray->pData + offset }
            {}
            ViewIterator(const ViewIterator& rhs) noexcept : pRoot{ rhs.pRoot }, pArray{ rhs.pArray } {}
            ViewIterator& operator=(const ViewIterator& rhs) noexcept
            {
                // Self-assignment check
                if (this != &rhs) {
                    pRoot = rhs.pRoot;
                    pArray = rhs.pArray;
                }
                return *this;
            }
            ViewIterator& operator+=(difference_type rhs) noexcept
            {
                pArray += rhs;
                return *this;
            }
            ViewIterator& operator-=(difference_type rhs) noexcept
            {
                pArray -= rhs;
                return *this;
            }
            value_type operator*() const noexcept
            {
                return value_type{ pRoot, *pArray };
            }
            value_type operator->() const noexcept
            {
                return **this;
            }
            value_type operator[](size_t idx) const noexcept
            {
                return value_type{ pRoot, pArray[idx] };
            }

            ViewIterator& operator++() noexcept
            {
                ++pArray;
                return *this;
            }
            ViewIterator& operator--() noexcept
            {
                --pArray;
                return *this;
            }
            ViewIterator operator++(int) noexcept { ViewIterator tmp(*this); ++(*this); return tmp; }
            ViewIterator operator--(int) noexcept { ViewIterator tmp(*this); --(*this); return tmp; }
            difference_type operator-(const ViewIterator& rhs) const noexcept
            {
                return difference_type(pArray - rhs.pArray);
            }
            ViewIterator operator+(difference_type rhs) const noexcept
            {
                auto dup = *this;
                return dup += rhs;
            }
            ViewIterator operator-(difference_type rhs) const noexcept
            {
                auto dup = *this;
                return dup -= rhs;
            }

            bool operator==(const ViewIterator& rhs) const noexcept { return pArray == rhs.pArray; }
            bool operator!=(const ViewIterator& rhs) const noexcept { return pArray != rhs.pArray; }
            bool operator>(const ViewIterator& rhs) const noexcept { return pArray > rhs.pArray; }
            bool operator<(const ViewIterator& rhs) const noexcept { return pArray < rhs.pArray; }
            bool operator>=(const ViewIterator& rhs) const noexcept { return pArray >= rhs.pArray; }
            bool operator<=(const ViewIterator& rhs) const noexcept { return pArray <= rhs.pArray; }
        private:
            // data
            const base_type* const* pArray = nullptr;
            const class Root* pRoot = nullptr;
        };

        template<class V>
        using ViewRange = std::ranges::subrange<ViewIterator<V>, ViewIterator<V>>;

        class EnumKeyView
        {
            using BaseType = PM_INTROSPECTION_ENUM_KEY;
            using SelfType = EnumKeyView;
            friend class ViewIterator<SelfType>;
            friend class Root;
        public:
            int GetId() const
            {
                return pBase->id;
            }
            std::string GetSymbol() const
            {
                return pBase->pSymbol->pData;
            }
            std::string GetName() const
            {
                return pBase->pName->pData;
            }
            std::string GetShortName() const
            {
                return pBase->pShortName->pData;
            }
            std::string GetDescription() const
            {
                return pBase->pDescription->pData;
            }
            const SelfType* operator->() const
            {
                return this;
            }
            const BaseType* GetBasePtr() const
            {
                return pBase;
            }
        private:
            // functions
            EnumKeyView(const class Root* pRoot_, const BaseType* pBase_)
                :
                pRoot{ pRoot_ },
                pBase{ pBase_ }
            {}
            // data
            const class Root* pRoot = nullptr;
            const BaseType* pBase = nullptr;
        };

        class EnumView
        {
            using BaseType = PM_INTROSPECTION_ENUM;
            using SelfType = EnumView;
            friend class ViewIterator<SelfType>;
            friend class Root;
        public:
            PM_ENUM GetId() const
            {
                return pBase->id;
            }
            std::string GetSymbol() const
            {
                return pBase->pSymbol->pData;
            }
            std::string GetDescription() const
            {
                return pBase->pDescription->pData;
            }
            ViewRange<EnumKeyView> GetKeys() const
            {
                // trying to deduce the template params for subrange causes intellisense to crash
                // workaround this by providing them explicitly as the return type (normally would use auto)
                return { GetKeysBegin_(), GetKeysEnd_() };
            }
            const SelfType* operator->() const
            {
                return this;
            }
            const BaseType* GetBasePtr() const
            {
                return pBase;
            }
        private:
            // functions
            EnumView(const class Root* pRoot_, const BaseType* pBase_)
                :
                pRoot{ pRoot_ },
                pBase{ pBase_ }
            {}
            ViewIterator<EnumKeyView> GetKeysBegin_() const
            {
                return { pRoot, pBase->pKeys };
            }
            ViewIterator<EnumKeyView> GetKeysEnd_() const
            {
                return { pRoot, pBase->pKeys, (int64_t)pBase->pKeys->size };
            }
            // data
            const class Root* pRoot;
            const BaseType* pBase = nullptr;
        };

        class DeviceView
        {
            using BaseType = PM_INTROSPECTION_DEVICE;
            using SelfType = DeviceView;
            friend class ViewIterator<SelfType>;
            friend class Root;
            friend class DeviceMetricInfoView;
        public:
            EnumKeyView IntrospectType() const;
            PM_DEVICE_TYPE GetType() const { return pBase->type; }
            EnumKeyView IntrospectVendor() const;
            PM_DEVICE_VENDOR GetVendor() const { return pBase->vendor; }
            uint32_t GetId() const
            {
                return pBase->id;
            }
            std::string GetName() const
            {
                return pBase->pName->pData;
            }
            const SelfType* operator->() const
            {
                return this;
            }
            const BaseType* GetBasePtr() const
            {
                return pBase;
            }
        private:
            // functions
            DeviceView(const class Root* pRoot_, const BaseType* pBase_)
                :
                pRoot{ pRoot_ },
                pBase{ pBase_ }
            {}
            // data
            const class Root* pRoot = nullptr;
            const BaseType* pBase = nullptr;
        };

        class DeviceMetricInfoView
        {
            using BaseType = PM_INTROSPECTION_DEVICE_METRIC_INFO;
            using SelfType = DeviceMetricInfoView;
            friend class ViewIterator<SelfType>;
            friend class Root;
        public:
            DeviceView GetDevice() const;
            EnumKeyView IntrospectAvailablity() const;
            PM_METRIC_AVAILABILITY GetAvailability() const { return pBase->availability; }
            uint32_t GetArraySize() const
            {
                return pBase->arraySize;
            }
            bool IsAvailable() const
            {
                return pBase->availability == PM_METRIC_AVAILABILITY_AVAILABLE;
            }
            const SelfType* operator->() const
            {
                return this;
            }
            const BaseType* GetBasePtr() const
            {
                return pBase;
            }
        private:
            // functions
            DeviceMetricInfoView(const class Root* pRoot_, const BaseType* pBase_)
                :
                pRoot{ pRoot_ },
                pBase{ pBase_ }
            {}
            // data
            const class Root* pRoot;
            const BaseType* pBase = nullptr;
        };

        class DataTypeInfoView
        {
            using BaseType = PM_INTROSPECTION_DATA_TYPE_INFO;
            using SelfType = DataTypeInfoView;
            friend class MetricView;
        public:
            EnumKeyView IntrospectPolledType() const;
            PM_DATA_TYPE GetPolledType() const { return pBase->polledType; }
            EnumKeyView IntrospectFrameType() const;
            PM_DATA_TYPE GetFrameType() const { return pBase->frameType; }
            EnumView IntrospectEnum() const;
            PM_ENUM GetEnumId() const
            {
                if (pBase->polledType != PM_DATA_TYPE_ENUM && pBase->frameType != PM_DATA_TYPE_ENUM) {
                    throw DatatypeException{ "cannot get enum data for non-enum data type" };
                }
                return pBase->enumId;
            }
            const SelfType* operator->() const
            {
                return this;
            }
            const BaseType* GetBasePtr() const
            {
                return pBase;
            }
        private:
            // functions
            DataTypeInfoView(const class Root* pRoot_, const BaseType* pBase_)
                :
                pRoot{ pRoot_ },
                pBase{ pBase_ }
            {}
            // data
            const class Root* pRoot = nullptr;
            const BaseType* pBase = nullptr;
        };

        class StatInfoView
        {
            using BaseType = PM_INTROSPECTION_STAT_INFO;
            using SelfType = StatInfoView;
            friend class ViewIterator<SelfType>;
            friend class MetricView;
        public:
            EnumKeyView IntrospectStat() const;
            PM_STAT GetStat() const { return pBase->stat; }
            const SelfType* operator->() const
            {
                return this;
            }
            const BaseType* GetBasePtr() const
            {
                return pBase;
            }
        private:
            // functions
            StatInfoView(const class Root* pRoot_, const BaseType* pBase_)
                :
                pRoot{ pRoot_ },
                pBase{ pBase_ }
            {}
            // data
            const class Root* pRoot = nullptr;
            const BaseType* pBase = nullptr;
        };

        class UnitView
        {
            using BaseType = PM_INTROSPECTION_UNIT;
            using SelfType = UnitView;
            friend class ViewIterator<SelfType>;
            friend class Root;
        public:
            EnumKeyView Introspect() const;
            PM_UNIT GetId() const;
            EnumKeyView IntrospectBaseUnit() const;
            PM_UNIT GetBaseUnit() const;
            double GetScale() const;
            double MakeConversionFactor(PM_UNIT destinationUnitId) const;
            const SelfType* operator->() const
            {
                return this;
            }
            const BaseType* GetBasePtr() const
            {
                return pBase;
            }
        private:
            // functions
            UnitView(const class Root* pRoot_, const BaseType* pBase_)
                :
                pRoot{ pRoot_ },
                pBase{ pBase_ }
            {}
            // data
            const class Root* pRoot;
            const BaseType* pBase = nullptr;
        };

        class MetricView
        {
            using BaseType = PM_INTROSPECTION_METRIC;
            using SelfType = MetricView;
            friend class ViewIterator<SelfType>;
            friend class Root;
        public:
            EnumKeyView Introspect() const;
            PM_METRIC GetId() const;
            EnumKeyView IntrospectUnit() const;
            PM_UNIT GetUnit() const;
            EnumKeyView IntrospectPreferredUnitHint() const;
            PM_UNIT GetPreferredUnitHint() const;
            EnumKeyView IntrospectType() const;
            PM_METRIC_TYPE GetType() const;
            DataTypeInfoView GetDataTypeInfo() const;
            ViewRange<StatInfoView> GetStatInfo() const;
            ViewRange<DeviceMetricInfoView> GetDeviceMetricInfo() const
            {
                // trying to deduce the template params for subrange causes intellisense to crash
                // workaround this by providing them explicitly as the return type (normally would use auto)
                return { GetDeviceMetricInfoBegin_(), GetDeviceMetricInfoEnd_() };
            }
            const SelfType* operator->() const
            {
                return this;
            }
            const BaseType* GetBasePtr() const
            {
                return pBase;
            }
        private:
            // functions
            ViewIterator<StatInfoView> GetStatInfoBegin_() const
            {
                return { pRoot, pBase->pStatInfo };
            }
            ViewIterator<StatInfoView> GetStatInfoEnd_() const
            {
                return { pRoot, pBase->pStatInfo, (int64_t)pBase->pStatInfo->size };
            }
            ViewIterator<DeviceMetricInfoView> GetDeviceMetricInfoBegin_() const
            {
                return { pRoot, pBase->pDeviceMetricInfo };
            }
            ViewIterator<DeviceMetricInfoView> GetDeviceMetricInfoEnd_() const
            {
                return { pRoot, pBase->pDeviceMetricInfo, (int64_t)pBase->pDeviceMetricInfo->size };
            }
            MetricView(const class Root* pRoot_, const BaseType* pBase_)
                :
                pRoot{ pRoot_ },
                pBase{ pBase_ }
            {}
            // data
            const class Root* pRoot;
            const BaseType* pBase = nullptr;
        };


        class Root
        {
        public:
            Root(const PM_INTROSPECTION_ROOT* pRoot_, std::function<void(const PM_INTROSPECTION_ROOT*)> deleter_)
                :
                pRoot{ pRoot_ },
                deleter{ std::move(deleter_) }
            {
                assert(pRoot);
                assert(deleter);
                // building lookup tables for enum/key
                for (auto e : GetEnums()) {
                    for (auto k : e.GetKeys()) {
                        enumKeyMap[MakeEnumKeyMapKey_(e.GetId(), k.GetId())] = k.GetBasePtr();
                    }
                    enumMap[e.GetId()] = e.GetBasePtr();
                }
                // building lookup table for devices
                for (auto d : GetDevices()) {
                    deviceMap[d.GetId()] = d.GetBasePtr();
                }
                // building lookup table for metrics
                for (auto m : GetMetrics()) {
                    metricMap[m.GetId()] = m.GetBasePtr();
                }
                // building lookup table for units
                for (auto u : GetUnits()) {
                    unitMap[u.GetId()] = u.GetBasePtr();
                }
            }
            ~Root()
            {
                if (pRoot) {
                    deleter(pRoot);
                }
            }
            Root(Root&& rhs) = delete;
            Root& operator=(Root&& rhs) = delete;
            ViewRange<EnumView> GetEnums() const
            {
                // trying to deduce the template params for subrange causes intellisense to crash
                // workaround this by providing them explicitly as the return type (normally would use auto)
                return { GetEnumsBegin_(), GetEnumsEnd_() };
            }
            ViewRange<MetricView> GetMetrics() const
            {
                // trying to deduce the template params for subrange causes intellisense to crash
                // workaround this by providing them explicitly as the return type (normally would use auto)
                return { GetMetricsBegin_(), GetMetricsEnd_() };
            }
            ViewRange<DeviceView> GetDevices() const
            {
                // trying to deduce the template params for subrange causes intellisense to crash
                // workaround this by providing them explicitly as the return type (normally would use auto)
                return { GetDevicesBegin_(), GetDevicesEnd_() };
            }
            ViewRange<UnitView> GetUnits() const
            {
                // trying to deduce the template params for subrange causes intellisense to crash
                // workaround this by providing them explicitly as the return type (normally would use auto)
                return { GetUnitsBegin_(), GetUnitsEnd_() };
            }
            EnumKeyView FindEnumKey(PM_ENUM enumId, int keyValue) const
            {
                if (auto i = enumKeyMap.find(MakeEnumKeyMapKey_(enumId, keyValue)); i == enumKeyMap.end()) {
                    throw LookupException{ std::format("unable to find key value={} for enum ID={}", keyValue, (int)enumId) };
                }
                else {
                    return { this, i->second };
                }
            }
            EnumView FindEnum(PM_ENUM enumId) const
            {
                if (auto i = enumMap.find(enumId); i == enumMap.end()) {
                    throw LookupException{ std::format("unable to find enum ID={}", (int)enumId) };
                }
                else {
                    return { this, i->second };
                }
            }
            DeviceView FindDevice(uint32_t deviceId) const
            {
                if (auto i = deviceMap.find(deviceId); i == deviceMap.end()) {
                    throw LookupException{ std::format("unable to find device ID={}", deviceId) };
                }
                else {
                    return { this, i->second };
                }
            }
            MetricView FindMetric(PM_METRIC metricId) const
            {
                if (auto i = metricMap.find(metricId); i == metricMap.end()) {
                    throw LookupException{ std::format("unable to find metric ID={}", (int)metricId) };
                }
                else {
                    return { this, i->second };
                }
            }
            UnitView FindUnit(PM_UNIT unitId) const
            {
                if (auto i = unitMap.find(unitId); i == unitMap.end()) {
                    throw LookupException{ std::format("unable to find unit ID={}", (int)unitId) };
                }
                else {
                    return { this, i->second };
                }
            }
        private:
            // functions
            static uint64_t MakeEnumKeyMapKey_(PM_ENUM enumId, int keyValue)
            {
                // pack 64-bit hash key as upper and lower 32-bit values
                return (uint64_t(enumId) << 32) | uint64_t(keyValue);
            }
            ViewIterator<EnumView> GetEnumsBegin_() const
            {
                return ViewIterator<EnumView>{ this, pRoot->pEnums };
            }
            ViewIterator<EnumView> GetEnumsEnd_() const
            {
                return ViewIterator<EnumView>{ this, pRoot->pEnums, (int64_t)pRoot->pEnums->size };
            }
            ViewIterator<MetricView> GetMetricsBegin_() const
            {
                return ViewIterator<MetricView>{ this, pRoot->pMetrics };
            }
            ViewIterator<MetricView> GetMetricsEnd_() const
            {
                return ViewIterator<MetricView>{ this, pRoot->pMetrics, (int64_t)pRoot->pMetrics->size };
            }
            ViewIterator<DeviceView> GetDevicesBegin_() const
            {
                return ViewIterator<DeviceView>{ this, pRoot->pDevices };
            }
            ViewIterator<DeviceView> GetDevicesEnd_() const
            {
                return ViewIterator<DeviceView>{ this, pRoot->pDevices, (int64_t)pRoot->pDevices->size };
            }
            ViewIterator<UnitView> GetUnitsBegin_() const
            {
                return ViewIterator<UnitView>{ this, pRoot->pUnits };
            }
            ViewIterator<UnitView> GetUnitsEnd_() const
            {
                return ViewIterator<UnitView>{ this, pRoot->pUnits, (int64_t)pRoot->pUnits->size };
            }
            // data
            const PM_INTROSPECTION_ROOT* pRoot = nullptr;
            std::function<void(const PM_INTROSPECTION_ROOT*)> deleter;
            std::unordered_map<uint64_t, const PM_INTROSPECTION_ENUM_KEY*> enumKeyMap;
            std::unordered_map<PM_ENUM, const PM_INTROSPECTION_ENUM*> enumMap;
            std::unordered_map<uint32_t, const PM_INTROSPECTION_DEVICE*> deviceMap;
            std::unordered_map<PM_METRIC, const PM_INTROSPECTION_METRIC*> metricMap;
            std::unordered_map<PM_UNIT, const PM_INTROSPECTION_UNIT*> unitMap;
        };
    }
}