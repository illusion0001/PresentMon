// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#include "AsyncEndpoint.h"
#include <Core/source/infra/Logging.h>

namespace p2c::client::util
{
	AsyncEndpoint::AsyncEndpoint(Environment env) : env{ env } {}

	void AsyncEndpoint::ExecuteOnBrowser(uint64_t uid, CefRefPtr<CefValue> pArgObj, CefRefPtr<CefBrowser> pBrowser) const
	{
		pmlog_warn(L"virtual not implemented");
	}

	void AsyncEndpoint::ExecuteOnRenderAccessor(uint64_t uid, CefRefPtr<CefValue> pArgObj, cef::DataBindAccessor& accessor) const
	{
		pmlog_warn(L"virtual not implemented");
	}

	AsyncEndpoint::Result AsyncEndpoint::ExecuteOnKernelTask(uint64_t uid, CefRefPtr<CefValue> pArgObj, kern::Kernel& kernel) const
	{
		pmlog_warn(L"virtual not implemented");
		return {};
	}

	AsyncEndpoint::Environment AsyncEndpoint::GetEnvironment() const { return env; }

	AsyncEndpoint::Result AsyncEndpoint::MakeStringErrorResult(std::wstring errorString)
	{
		auto str = CefValue::Create();
		str->SetString(errorString);
		return { false, std::move(str) };
	}
}