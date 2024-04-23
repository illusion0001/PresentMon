// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#include <windows.h>
#include <string>
#include "Console.h"
#include <stdint.h>
#include <iostream>
#include <fstream>
#include <thread>
#include <TlHelp32.h>
#include <iostream>
#include <iomanip>
#include <vector>
#include <algorithm>
#include <format>
#include <chrono>
#include <conio.h>
#include "../PresentMonAPI2/PresentMonAPI.h"
#include "../PresentMonAPI2/Internal.h"
#include "CliOptions.h"

#include "../PresentMonAPIWrapper/PresentMonAPIWrapper.h"
#include "../PresentMonAPIWrapper/FixedQuery.h"
#include "Utils.h"
#include "DynamicQuerySample.h"
#include "FrameQuerySample.h"
#include "IntrospectionSample.h"
#include "CheckMetricSample.h"
#include "WrapperStaticQuery.h"
#include "MetricListSample.h"
#include "../PresentMonAPIWrapperCommon/PmErrorCodeProvider.h"

#define PMLOG_BUILD_LEVEL ::pmon::util::log::Level::Verbose
#include "../CommonUtilities/log/Log.h"
#include "../CommonUtilities/log/NamedPipeMarshallReceiver.h"
#include "../CommonUtilities/log/NamedPipeMarshallSender.h"
#include "../CommonUtilities/log/MarshallDriver.h"
#include "../CommonUtilities/log/EntryMarshallInjector.h"
#include "../CommonUtilities/log/IdentificationTable.h"
#include "../CommonUtilities/log/LineTable.h"
#include "../CommonUtilities/Exception.h"
#include "../CommonUtilities/win/Utilities.h"

using namespace pmon;
using namespace pmon::util;

struct Test
{
    Test()
    {
        pmlog_info(L"global init log");
    }
    __declspec(noinline) ~Test()
    {
        pmlog_error(L"global destroy log w/ trace");
    }
};

struct LogBooter { LogBooter() { pmon::util::log::GetDefaultChannel(); } };

struct
{
    LogBooter lb;
    Test t;
} glob;

void f() {
    pmlog_error().code(PM_STATUS_SERVICE_ERROR);
}
void g() {
    f();
}

PM_DEFINE_EX(MyException);

void j() {
    throw Except<MyException>("fine time to dine");
}
void k() {
    j();
}

class SE_Exception : public Exception
{
private:
    const unsigned int nSE;
public:
    SE_Exception() noexcept : SE_Exception{ 0 } {}
    SE_Exception(unsigned int n) noexcept : nSE{ n } {}
    unsigned int getSeNumber() const noexcept { return nSE; }
protected:
    std::string ComposeWhatString_() const noexcept override
    {
        try {
            std::ostringstream oss;
            oss << std::format("Error Code [0x{:08X}]\n", getSeNumber());
            if (HasTrace_()) {
                oss << "\n" << GetTraceString_();
            }
            return oss.str();
        }
        catch (...) {}
        return {};
    }
};

void seh_trans_func(unsigned int u, EXCEPTION_POINTERS*)
{
    throw SE_Exception{ u };
}

int main(int argc, char* argv[])
{
    pmlog_setup;

    try {
        if (auto e = clio::Options::Init(argc, argv)) {
            return *e;
        }
        auto& opt = clio::Options::Get();

        using namespace pmon::util;
        using namespace std::chrono_literals;

        pmon::util::log::GlobalPolicy::SetLogLevel(pmon::util::log::Level::Verbose);
        try {
            pmon::util::log::LineTable::IngestList(L"black.txt");
            pmon::util::log::LineTable::SetListMode(pmon::util::log::LineTable::ListMode::Black);
            pmon::util::log::LineTable::SetTraceOverride(true);
        }
        catch (...) { std::cout << "cant find black.txt\n"; }

        g();

        std::jthread{ [] {
            g();
        } };

        if (opt.doPipeSrv) {
            log::IdentificationTable::AddThisProcess(L"p-server");
            log::IdentificationTable::AddThisThread(L"t-main");
            {
                auto pSender = std::make_shared<log::NamedPipeMarshallSender>(L"pml_testpipe");
                auto pDriver = std::make_shared<log::MarshallDriver>(std::move(pSender));
                log::GetDefaultChannel()->AttachDriver(std::move(pDriver));
            }
            std::wstring note;
            while (true) {
                int x = 3;
                std::cout << "SAY> ";
                std::getline(std::wcin, note);
                pmlog_info(note).pmwatch(x+2).every(3);
                if (note == L"@#$") {
                    break;
                }
            }
            return 0;
        }
        if (opt.doPipeCli) {
            log::IdentificationTable::AddThisProcess(L"p-client");
            log::IdentificationTable::AddThisThread(L"t-main");
            auto pReceiver = std::make_shared<log::NamedPipeMarshallReceiver>(L"pml_testpipe", log::IdentificationTable::GetPtr());
            log::EntryMarshallInjector injector{ log::GetDefaultChannel(), std::move(pReceiver) };
            while (!_kbhit());
            return 0;
        }

        // validate options, better to do this with CLI11 validation but framework needs upgrade...
        if (bool(opt.controlPipe) != bool(opt.introNsm)) {
            std::cout << "Must set both control pipe and intro NSM, or neither.\n";
            return -1;
        }

        pmlog_error(L"henlo");

        // determine requested activity
        if (opt.introspectionSample ^ opt.dynamicQuerySample ^ opt.frameQuerySample ^ opt.checkMetricSample ^ opt.wrapperStaticQuerySample ^ opt.metricListSample) {
            std::unique_ptr<pmapi::Session> pSession;
            if (opt.controlPipe) {
                pSession = std::make_unique<pmapi::Session>(*opt.controlPipe, *opt.introNsm);
            }
            else {
                pSession = std::make_unique<pmapi::Session>();
            }

            pmlog_info().code(PM_STATUS_DATA_LOSS);

            struct Test { Test() { std::cout << "hiya!" << std::endl; }
                ~Test() { std::cout << "Byeee" << std::endl; } } test;

            _set_se_translator(seh_trans_func);
            log::GlobalPolicy::SetExceptionTracePolicy(log::ExceptionTracePolicy::OverrideOn);
            RaiseException(STATUS_INVALID_HANDLE, 0, 0, nullptr);
            k();

            if (opt.introspectionSample) {
                return IntrospectionSample(std::move(pSession));
            }
            else if (opt.checkMetricSample) {
                return CheckMetricSample(std::move(pSession));
            }
            else if (opt.dynamicQuerySample) {
                return DynamicQuerySample(std::move(pSession), *opt.windowSize, *opt.metricOffset);
            }
            else if (opt.wrapperStaticQuerySample) {
                return WrapperStaticQuerySample(std::move(pSession));
            }
            else if (opt.metricListSample) {
                return MetricListSample(*pSession);
            }
            else {
                return FrameQuerySample(std::move(pSession));
            }
        }
        else {
            std::cout << "SampleClient supports one action at a time. Select one of:\n";
            std::cout << "--introspection-sample\n";
            std::cout << "--wrapper-static-query-sample\n";
            std::cout << "--dynamic-query-sample [--process-id id | --process-name name.exe] [--add-gpu-metric]\n";
            std::cout << "--frame-query-sample [--process-id id | --process-name name.exe]  [--gen-csv]\n";
            std::cout << "--check-metric-sample --metric PM_METRIC_*\n";
            return -1;
        }
    }
    catch (...) {
        std::cout << "Exception: " << ReportException();
        return -1;
    }
}