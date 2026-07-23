//========================================================================================
//
//  Owner: KohakuNekotarou
//
//  KohakuBookSearch (KBS)
//
//  Jump-marker expiry timer (KBSMarkerExpiryIdleTask.h). One-shot: it fires once, clears the
//  marker and takes itself off the queue. Ported from KESCL's KESCLMarkerExpiryIdleTask - the
//  install/uninstall ordering and the shutdown retire are kept exactly (they are the robust,
//  crash-free teardown this whole task exists to preserve).
//
//========================================================================================

#include "VCPlugInHeaders.h"

#include "CIdleTask.h"
#include "IIdleTaskMgr.h"

#include "KBSID.h"
#include "KBSMarkerExpiryIdleTask.h"
#include "KBSDrawEventHandler.h"

// How long the marker stays up. Short on purpose: it points at the hit, and the view has already
// been scrolled so the hit is on screen anyway.
static const uint32 kKBSMarkerLifetimeMs = 1000;

// ---- Shared state (private to this translation unit) ----
static IIdleTask* sTask     = nil;		// the task object (created once, reused). Released in Shutdown
static bool16     sRunning  = kFalse;	// is the task currently sitting in the idle queue?
static bool16     sShutdown = kFalse;	// set at application shutdown: never create/schedule again

//========================================================================================
// KBSMarkerExpiryTask - minimal CIdleTask: only RunTask / TaskName. InstallTask / UninstallTask
// come from CIdleTask (which calls the IdleTaskMgr's AddTask / RemoveTask). Named differently
// from the KBSMarkerExpiryIdleTask namespace, which holds the public entry points.
//========================================================================================
class KBSMarkerExpiryTask : public CIdleTask
{
public:
	KBSMarkerExpiryTask(IPMUnknown* boss) : CIdleTask(boss) {}

	virtual uint32 RunTask(uint32 flags, IdleTimer* idleTimer);
	virtual const char* TaskName() { return "KBSMarkerExpiryTask"; }
};

CREATE_PMINTERFACE(KBSMarkerExpiryTask, kKBSMarkerExpiryIdleTaskImpl)

uint32 KBSMarkerExpiryTask::RunTask(uint32 /*flags*/, IdleTimer* /*idleTimer*/)
{
	// Take ourselves off the queue BEFORE clearing: ClearMarker calls back into
	// KBSMarkerExpiryIdleTask::Stop(), and clearing sRunning first makes that a no-op instead of a
	// second UninstallTask.
	sRunning = kFalse;
	this->UninstallTask();

	if (!sShutdown)
		KBSDrawEventHandler::ClearMarker();

	return 0;	// one-shot: nothing more to do
}

//========================================================================================
// Public entry points
//========================================================================================
void KBSMarkerExpiryIdleTask::Start()
{
	if (sShutdown)
		return;

	if (sTask == nil)
		sTask = ::CreateObject2<IIdleTask>(kKBSMarkerExpiryIdleTaskBoss);
	if (sTask == nil)
		return;		// no timer; the marker simply stays up until the next jump/search clears it.

	// Restart rather than let a running countdown stand: the marker was just (re)shown, so it is
	// owed the full lifetime from now. InstallTask on an already-installed task does nothing, so
	// the pending one has to come off first.
	if (sRunning)
		sTask->UninstallTask();

	sTask->InstallTask(kKBSMarkerLifetimeMs);
	sRunning = kTrue;
}

void KBSMarkerExpiryIdleTask::Stop()
{
	if (sTask != nil && sRunning)
		sTask->UninstallTask();
	sRunning = kFalse;
}

void KBSMarkerExpiryIdleTask::Shutdown()
{
	sShutdown = kTrue;	// no re-arming from here on
	if (sTask != nil)
	{
		if (sRunning)
			sTask->UninstallTask();
		sRunning = kFalse;
		sTask->Release();
		sTask = nil;
	}
}

// End, KBSMarkerExpiryIdleTask.cpp.
