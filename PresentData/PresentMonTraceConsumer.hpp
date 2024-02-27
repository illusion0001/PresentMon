// Copyright (C) 2017-2022 Intel Corporation
// SPDX-License-Identifier: MIT
#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <stdint.h>
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>
#include <set>
#include <windows.h>
#include <evntcons.h> // must include after windows.h

#include "Debug.hpp"
#include "GpuTrace.hpp"
#include "TraceConsumer.hpp"

// PresentMode represents the different paths a present can take on windows.
//
// Hardware_Legacy_Flip:
//     Runtime PresentStart
//     -> Flip (by thread/process, for classification)
//     -> QueueSubmit (by thread, for submit sequence)
//     -> MMIOFlip (by submit sequence, for ready time and immediate flags)
//     -> VSyncDPC (by submit sequence, for screen time)
//
// Hardware_Legacy_Copy_To_Front_Buffer:
//     Runtime PresentStart
//     -> DxgKrnl_Blit (by thread/process, for classification)
//     -> QueueSubmit (by thread, for submit sequence)
//     -> QueueComplete (by submit sequence, indicates ready and screen time)
// Distinction between FS and windowed blt is done by the lack of other events.
//
// Hardware_Independent_Flip:
//     Follows same path as Composed_Flip, but TokenStateChanged indicates IndependentFlip
//     -> MMIOFlip (by submit sequence, for immediate flags)
//     -> VSyncDPC or HSyncDPC (by submit sequence, for screen time)
//
// Composed_Flip (FLIP_SEQUENTIAL, FLIP_DISCARD, FlipEx):
//     Runtime PresentStart
//     -> TokenCompositionSurfaceObject (by thread/process, for classification and token key)
//     -> DxgKrnl_PresentHistoryDetailed (by thread, for token ptr)
//     -> QueueSubmit (by thread, for submit sequence)
//     -> DxgKrnl_PresentHistory_Info (by token ptr, for ready time) and TokenStateChanged (by token key, for discard status and intent to present)
//     -> DWM Present (consumes most recent present per hWnd, marks DWM thread ID)
//     -> A fullscreen present is issued by DWM, and when it completes, this present is on screen
//
// Composed_Copy_with_GPU_GDI (a.k.a. Win7 Blit):
//     Runtime PresentStart
//     -> DxgKrnl_Blit (by thread/process, for classification)
//     -> DxgKrnl_PresentHistoryDetailed (by thread, for token ptr and classification)
//     -> DxgKrnl_Present (by thread, for hWnd)
//     -> DxgKrnl_PresentHistory_Info (by token ptr, for ready time)
//     -> DWM UpdateWindow (by hWnd, marks hWnd active for composition)
//     -> DWM Present (consumes most recent present per hWnd, marks DWM thread ID)
//     -> A fullscreen present is issued by DWM, and when it completes, this present is on screen
//
// Composed_Copy_with_CPU_GDI (a.k.a. Vista Blit):
//     Runtime PresentStart
//     -> DxgKrnl_Blit (by thread/process, for classification)
//     -> DxgKrnl_PresentHistory_Start (by thread, for token ptr, legacy blit token, and classification)
//     -> DxgKrnl_PresentHistory_Info (by token ptr, for ready time)
//     -> DWM FlipChain (by legacy blit token, for hWnd and marks hWnd active for composition)
//     -> Follows the Windowed_Blit path for tracking to screen
//
// Hardware_Composed_Independent_Flip:
//     Identical to hardware independent flip, but VSyncDPCMPO and HSyncDPCMPO contains more than one valid plane and SubmitSequence.
//
// The following present modes are not currently detected by PresentMon:
//
// Hardware_Direct_Flip:
//     Not uniquely detectable through ETW (follows the same path as Composed_Flip)
//
// Composed Composition Atlas (DirectComposition):
//     Unable to track composition dependencies, leading to incorrect/misleading metrics.
//     Runtime PresentStart
//     -> DxgKrnl_PresentHistory_Start (use model field for classification, get token ptr)
//     -> DxgKrnl_PresentHistory_Info (by token ptr)
//     -> Assume DWM will compose this buffer on next present (missing InFrame event), follow windowed blit paths to screen time

enum class PresentMode {
    Unknown = 0,
    Hardware_Legacy_Flip = 1,
    Hardware_Legacy_Copy_To_Front_Buffer = 2,
    Hardware_Independent_Flip = 3,
    Composed_Flip = 4,
    Composed_Copy_GPU_GDI = 5,
    Composed_Copy_CPU_GDI = 6,
    Hardware_Composed_Independent_Flip = 8,
};

enum class PresentResult {
    Unknown = 0,
    Presented = 1,
    Discarded = 2,
};

enum class Runtime {
    Other = 0,
    DXGI = 1,
    D3D9 = 2,
};

enum class InputDeviceType {
    None = 0,
    Unknown = 1,
    Mouse = 2,
    Keyboard = 3,
};

struct InputEvent {
    uint64_t Time;
    InputDeviceType Type;
};

// A ProcessEvent occurs whenever a Process starts or stops.
struct ProcessEvent {
    std::wstring ImageFileName; // The name of the process exe file.  This is only available on process start events.
    uint64_t QpcTime;           // The time of the start/stop event.
    uint32_t ProcessId;         // The id of the process.
    bool IsStartEvent;          // Whether this is a start event (true) or a stop event (false).
};

struct PresentEvent {
    uint64_t PresentStartTime;  // QPC value of the first event related to the Present (D3D9, DXGI, or DXGK Present_Start)
    uint32_t ProcessId;         // ID of the process that presented
    uint32_t ThreadId;          // ID of the thread that presented
    uint64_t PresentStopTime;   // QPC value when the application's present call completed
    uint64_t GPUStartTime;      // QPC value when the frame's first DMA packet started
    uint64_t ReadyTime;         // QPC value when the frame's last DMA packet completed
    uint64_t GPUDuration;       // QPC duration during which a frame's DMA packet was running on
                                // ... any node (if mTrackGPUVideo==false) or non-video nodes (if mTrackGPUVideo==true)
    uint64_t GPUVideoDuration;  // QPC duration during which a frame's DMA packet was running on a video node (if mTrackGPUVideo==true)
    uint64_t ScreenTime;        // QPC value when the present was displayed on screen
    uint64_t InputTime;         // Earliest QPC value when the keyboard/mouse was clicked and used by this frame

    // Extra present parameters obtained through DXGI or D3D9 present
    uint64_t SwapChainAddress;
    uint64_t PresentFrameCount; // since PresentMon started on boot
    int32_t SyncInterval;
    uint32_t PresentFlags;

    // Keys used to index into PMTraceConsumer's tracking data structures:
    uint64_t CompositionSurfaceLuid;      // mPresentByWin32KPresentHistoryToken
    uint64_t Win32KPresentCount;          // mPresentByWin32KPresentHistoryToken
    uint64_t Win32KBindId;                // mPresentByWin32KPresentHistoryToken
    uint64_t DxgkPresentHistoryToken;     // mPresentByDxgkPresentHistoryToken
    uint64_t DxgkPresentHistoryTokenData; // mPresentByDxgkPresentHistoryTokenData
    uint64_t DxgkContext;                 // mPresentByDxgkContext
    uint64_t Hwnd;                        // mLastPresentByWindow
    uint32_t QueueSubmitSequence;         // mPresentBySubmitSequence
    uint32_t mAllPresentsTrackingIndex;   // mAllPresents.
    // Note: the following index tracking structures as well but are defined elsewhere:
    //       ProcessId                 -> mOrderedPresentsByProcessId
    //       ThreadId, DriverThreadId  -> mPresentByThreadId
    //       PresentInDwmWaitingStruct -> mPresentsWaitingForDWM

    // How many PresentStop events from the thread to wait for before
    // enqueueing this present.
    uint32_t DeferredCompletionWaitCount;

    // Properties deduced by watching events through present pipeline
    uint32_t DestWidth;
    uint32_t DestHeight;
    uint32_t DriverThreadId;    // If the present is deferred by the driver, this will hold the
                                // threaad id that the driver finally presented on.
    Runtime Runtime;
    PresentMode PresentMode;
    PresentResult FinalState;
    InputDeviceType InputType;
    bool SupportsTearing;
    bool WaitForFlipEvent;
    bool WaitForMPOFlipEvent;
    bool SeenDxgkPresent;
    bool SeenWin32KEvents;
    bool DwmNotified;
    bool SeenInFrameEvent;      // This present has gotten a Win32k TokenStateChanged event into InFrame state
    bool GpuFrameCompleted;     // This present has already seen an event that caused GpuTrace::CompleteFrame() to be called.
    bool IsCompleted;           // All expected events have been observed
    bool IsLost;                // This PresentEvent was found in an unexpected state or is too old

    bool PresentInDwmWaitingStruct; // Whether this PresentEvent is currently stored in
                                    // PMTraceConsumer::mPresentsWaitingForDWM

    // Additional transient tracking state
    std::deque<std::shared_ptr<PresentEvent>> DependentPresents;

    // Track the path the present took through the PresentMon analysis.
    #ifdef TRACK_PRESENT_PATHS
    uint64_t AnalysisPath;
    #endif

    // Give every present a unique id for debugging.
    #if PRESENTMON_ENABLE_DEBUG_TRACE
    uint64_t Id;
    #endif

    PresentEvent();

private:
    PresentEvent(PresentEvent const& copy); // dne
};

struct PMTraceConsumer
{
    PMTraceConsumer();

    EventMetadata mMetadata;

    bool mFilteredEvents = false;       // Whether the trace session was configured to filter non-PresentMon events
    bool mFilteredProcessIds = false;   // Whether to filter presents to specific processes
    bool mTrackDisplay = true;          // Whether the analysis should track presents to display
    bool mTrackGPU = false;             // Whether the analysis should track GPU work
    bool mTrackGPUVideo = false;        // Whether the analysis should track GPU video work separately
    bool mTrackInput = false;           // Whether to track keyboard/mouse click times

    // Whether we've completed any presents yet.  This is used to indicate that
    // all the necessary providers have started and it's safe to start tracking
    // presents.
    bool mHasCompletedAPresent = false;

    // Store the DWM process id, and the last DWM thread id to have started a
    // present.  This is needed to determine if a flip event is coming from
    // DWM, but can also be useful for targetting non-DWM processes.
    //
    // mPresentsWaitingForDWM stores all in-progress presents that have been
    // handed off to DWM.  Once the next DWM present is detected, they are
    // added as its' DependentPresents.

    uint32_t DwmProcessId = 0;
    uint32_t DwmPresentThreadId = 0;
    // Whenever a present is completed, we save it here.
    // Because `present->PresentFrameCount++;` doesn't work for some reason.
    uint64_t mPresentFrames = 0;

    std::deque<std::shared_ptr<PresentEvent>> mPresentsWaitingForDWM;

    // Limit tracking to specified processes
    std::set<uint32_t> mTrackedProcessFilter;
    std::shared_mutex mTrackedProcessFilterMutex;

    // Storage for passing present path tracking id to Handle...() functions.
    #ifdef TRACK_PRESENT_PATHS
    uint32_t mAnalysisPathID;
    #endif


    // These store present and process events that are ready for the caller via
    // one of the thread-safe Dequeue*Events() functions.
    //
    // Completed presents have seen all their expected events, based on the
    // presentation path used.
    //
    // Lost presents were determined to be in an unexpected state, most-likely
    // caused by a missed ETW event (IsLost==true).

    std::mutex mPresentEventMutex;
    std::vector<std::shared_ptr<PresentEvent>> mCompletePresentEvents;

    std::mutex mLostPresentEventMutex;
    std::vector<std::shared_ptr<PresentEvent>> mLostPresentEvents;

    std::mutex mProcessEventMutex;
    std::vector<ProcessEvent> mProcessEvents;

    void DequeuePresentEvents(std::vector<std::shared_ptr<PresentEvent>>& outPresentEvents)
    {
        std::lock_guard<std::mutex> lock(mPresentEventMutex);
        outPresentEvents.swap(mCompletePresentEvents);
    }

    void DequeueLostPresentEvents(std::vector<std::shared_ptr<PresentEvent>>& outPresentEvents)
    {
        std::lock_guard<std::mutex> lock(mLostPresentEventMutex);
        outPresentEvents.swap(mLostPresentEvents);
    }

    void DequeueProcessEvents(std::vector<ProcessEvent>& outProcessEvents)
    {
        std::lock_guard<std::mutex> lock(mProcessEventMutex);
        outProcessEvents.swap(mProcessEvents);
    }


    // These data structures store in-progress presents that are being
    // processed by PMTraceConsumer.
    //
    // mAllPresents is a circular buffer storage for all in-progress presents.
    // Presents that are still in-progress when the buffer wraps are considered
    // lost due to age.
    //
    // mPresentByThreadId stores the in-progress present that was last operated
    // on by each thread.  This is used to look up the right present for event
    // sequences that are known to execute on the same thread.  The present
    // should be removed once those sequences are complete.
    //
    // mOrderedPresentsByProcessId stores each process' in-progress presents in
    // the order that they were created.  This is used to look up presents for
    // event sequences across different threads of the process (e.g., DXGI,
    // DXGK, driver threads).  It's also used to detect discarded presents when
    // newer presents are displayed from the same swapchain.
    //
    // mPresentBySubmitSequence stores presents who have had a present packet
    // submitted on to a queue until they are completed or discarded.  It's
    // used to associate those presents to various DXGK events (such as
    // MMIOFlip, IndependentFlip and *SyncDPC) which reference the submit
    // sequence id.
    //
    // mPresentByWin32KPresentHistoryToken stores the in-progress present
    // associated with each Win32KPresentHistoryToken, which is a unique key
    // used to identify all flip model presents, during composition.  Presents
    // should be removed once they have been confirmed.
    //
    // mPresentByDxgkPresentHistoryToken stores the in-progress present
    // associated with each DxgKrnl present history token, which is a unique
    // key used to identify all windowed presents. Presents should be removed
    // on DxgKrnl_Event_PropagatePresentHistory, which signals hand-off to DWM.
    //
    // mPresentByDxgkPresentHistoryTokenData stores the in-progress present
    // associated with a DxgKrnl->DWM token used only for Composed_Copy_CPU_GDI
    // presents.
    //
    // mPresentByDxgkContext stores the in-progress present associated with
    // each DxgContext.  It's only used for
    // Hardware_Legacy_Copy_To_Front_Buffer presents on Win7, and is needed to
    // distinguish between DWM-off fullscreen blts and the DWM-on blt to
    // redirection bitmaps.  The present is removed on the next queue
    // submisison.
    //
    // mLastPresentByWindow stores the latest in-progress present handed off to
    // DWM from each window.  It's needed to discard some legacy blts, which
    // don't always get a Win32K token Discarded transition.  The present is
    // either overwritten, or removed when DWM confirms the present.

    using OrderedPresents = std::map<uint64_t, std::shared_ptr<PresentEvent>>;

    using Win32KPresentHistoryToken = std::tuple<uint64_t, uint64_t, uint64_t>; // (composition surface pointer, present count, bind id)
    struct Win32KPresentHistoryTokenHash : private std::hash<uint64_t> {
        std::size_t operator()(Win32KPresentHistoryToken const& v) const noexcept;
    };

    unsigned int mAllPresentsNextIndex = 0;
    std::vector<std::shared_ptr<PresentEvent>> mAllPresents;

    std::unordered_map<uint32_t, std::shared_ptr<PresentEvent>> mPresentByThreadId;                     // ThreadId -> PresentEvent
    std::unordered_map<uint32_t, OrderedPresents>               mOrderedPresentsByProcessId;            // ProcessId -> ordered PresentStartTime -> PresentEvent
    std::unordered_map<uint32_t, std::unordered_map<uint64_t, std::shared_ptr<PresentEvent>>>
                                                                mPresentBySubmitSequence;               // SubmitSequenceId -> hContext -> PresentEvent
    std::unordered_map<Win32KPresentHistoryToken, std::shared_ptr<PresentEvent>,
                       Win32KPresentHistoryTokenHash>           mPresentByWin32KPresentHistoryToken;    // Win32KPresentHistoryToken -> PresentEvent
    std::unordered_map<uint64_t, std::shared_ptr<PresentEvent>> mPresentByDxgkPresentHistoryToken;      // DxgkPresentHistoryToken -> PresentEvent
    std::unordered_map<uint64_t, std::shared_ptr<PresentEvent>> mPresentByDxgkPresentHistoryTokenData;  // DxgkPresentHistoryTokenData -> PresentEvent
    std::unordered_map<uint64_t, std::shared_ptr<PresentEvent>> mPresentByDxgkContext;                  // DxgkContex -> PresentEvent
    std::unordered_map<uint64_t, std::shared_ptr<PresentEvent>> mLastPresentByWindow;                   // HWND -> PresentEvent


    // Once an in-progress present becomes lost, discarded, or displayed, it is
    // removed from all of the above tracking structures and moved into
    // mDeferredCompletions.
    //
    // In some cases (e.g., a present being displayed before Present() returns)
    // such presents have not yet seen all of their expected events.  When this
    // happens, the present will remain in mDeferredCompletions for
    // DeferredCompletionWaitCount PresentStop events from the same thread,
    // before being enqueued for the user.
    //
    // When all expected events are observed, or the
    // DeferredCompletionWaitCount expires, Presents are moved from
    // mDeferedCompletions into either mCompletePresentEvents or
    // mLostPresentEvents for the user to dequeue.

    struct DeferredCompletions {
        OrderedPresents mOrderedPresents;
        uint64_t mLastEnqueuedQpcTime;
    };

    std::unordered_map<uint32_t, std::unordered_map<uint64_t,
                                        DeferredCompletions>> mDeferredCompletions;   // ProcessId -> SwapChainAddress -> DeferredCompletions


    // mGpuTrace tracks work executed on the GPU.
    GpuTrace mGpuTrace;


    // State for tracking keyboard/mouse click times
    //
    // mLastInputDeviceReadTime and mLastInputDeviceType are the time/type of
    // the most-recent input event.  This is global and if multiple input
    // events are observed, but not retrieved by any window, then older ones
    // are lost.
    //
    // mRetrievedInput stores the time/type of the most-recent input event that
    // has been retrieved by each process's window(s).  Once that data is
    // applied to a present, the InputDeviceType is set to None, but the time
    // is not changed so that we know whether mLastInputDeviceReadTime has
    // already been retrieved or not.
    uint64_t mLastInputDeviceReadTime;
    InputDeviceType mLastInputDeviceType;

    std::unordered_map<uint32_t, std::pair<uint64_t, InputDeviceType>> mRetrievedInput; // ProcessID -> <InputTime, InputType>


    void HandleDxgkBlt(EVENT_HEADER const& hdr, uint64_t hwnd, bool redirectedPresent);
    void HandleDxgkFlip(EVENT_HEADER const& hdr, int32_t flipInterval, bool isMMIOFlip, bool isMPOFlip);
    void HandleDxgkQueueSubmit(EVENT_HEADER const& hdr, uint64_t hContext, uint32_t submitSequence, uint32_t packetType, bool isPresentPacket, bool isWin7);
    void HandleDxgkQueueComplete(uint64_t timestamp, uint64_t hContext, uint32_t submitSequence);
    void HandleDxgkMMIOFlip(uint64_t timestamp, uint32_t submitSequence, uint32_t flags);
    void HandleDxgkSyncDPC(uint64_t timestamp, uint32_t submitSequence);
    void HandleDxgkPresentHistory(EVENT_HEADER const& hdr, uint64_t token, uint64_t tokenData, Microsoft_Windows_DxgKrnl::PresentModel presentModel);
    void HandleDxgkPresentHistoryInfo(EVENT_HEADER const& hdr, uint64_t token);

    void CompletePresent(std::shared_ptr<PresentEvent> const& p);
    void CompletePresentHelper(std::shared_ptr<PresentEvent> const& p);
    void EnqueueDeferredCompletions(DeferredCompletions* deferredCompletions);
    void EnqueueDeferredPresent(std::shared_ptr<PresentEvent> const& p);
    void TrackPresent(std::shared_ptr<PresentEvent> present, OrderedPresents* presentsByThisProcess);
    void RemoveLostPresent(std::shared_ptr<PresentEvent> present);
    void RemovePresentFromTemporaryTrackingCollections(std::shared_ptr<PresentEvent> const& present);
    void RemovePresentFromSubmitSequenceIdTracking(std::shared_ptr<PresentEvent> const& present);
    void RuntimePresentStart(Runtime runtime, EVENT_HEADER const& hdr, uint64_t swapchainAddr, uint32_t dxgiPresentFlags, int32_t syncInterval);
    void RuntimePresentStop(Runtime runtime, EVENT_HEADER const& hdr, uint32_t result);

    void HandleProcessEvent(EVENT_RECORD* pEventRecord);
    void HandleDXGIEvent(EVENT_RECORD* pEventRecord);
    void HandleD3D9Event(EVENT_RECORD* pEventRecord);
    void HandleDXGKEvent(EVENT_RECORD* pEventRecord);
    void HandleWin32kEvent(EVENT_RECORD* pEventRecord);
    void HandleDWMEvent(EVENT_RECORD* pEventRecord);
    void HandleMetadataEvent(EVENT_RECORD* pEventRecord);

    void HandleWin7DxgkBlt(EVENT_RECORD* pEventRecord);
    void HandleWin7DxgkFlip(EVENT_RECORD* pEventRecord);
    void HandleWin7DxgkPresentHistory(EVENT_RECORD* pEventRecord);
    void HandleWin7DxgkQueuePacket(EVENT_RECORD* pEventRecord);
    void HandleWin7DxgkVSyncDPC(EVENT_RECORD* pEventRecord);
    void HandleWin7DxgkMMIOFlip(EVENT_RECORD* pEventRecord);

    void AddTrackedProcessForFiltering(uint32_t processID);
    void RemoveTrackedProcessForFiltering(uint32_t processID);
    bool IsProcessTrackedForFiltering(uint32_t processID);

    void SetThreadPresent(uint32_t threadId, std::shared_ptr<PresentEvent> const& present);
    std::shared_ptr<PresentEvent> FindThreadPresent(uint32_t threadId);
    std::shared_ptr<PresentEvent> FindOrCreatePresent(EVENT_HEADER const& hdr);
    std::shared_ptr<PresentEvent> FindPresentBySubmitSequence(uint32_t submitSequence);
};
