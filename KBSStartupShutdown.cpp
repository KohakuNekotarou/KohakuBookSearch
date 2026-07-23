//========================================================================================
//
//  Owner: KohakuNekotarou
//
//  KohakuBookSearch (KBS)
//
//  Startup/shutdown service. Its job is to retire the marker idle task and empty the module's
//  file-static state during InDesign's controlled shutdown (on the main thread), so nothing can
//  fire into - or be destructed against - a half-torn-down application at DLL unload. Ported from
//  KESCL's KESCLStartupShutdown, minus the Excel machinery KBS does not have.
//
//========================================================================================

#include "VCPlugInHeaders.h"

// Interface includes:
#include "IStartupShutdownService.h"

// General includes:
#include "CPMUnknown.h"

// Project includes:
#include "KBSID.h"
#include "KBSMarkerExpiryIdleTask.h"
#include "KBSBookScope.h"
#include "KBSResultModel.h"

/** Implements IStartupShutdownService for the plug-in. */
class KBSStartupShutdown : public CPMUnknown<IStartupShutdownService>
{
public:
	KBSStartupShutdown(IPMUnknown* boss) : CPMUnknown<IStartupShutdownService>(boss) {}
	virtual ~KBSStartupShutdown() {}

	/** Nothing to do at startup - the panel and the draw-event service are resource-driven. */
	virtual void Startup() {}

	/** Retire the marker idle task (it must leave the queue, and never be re-created, before the
	    app tears down) and release the module's static storage, so every static destructor at DLL
	    unload finds nothing left to do. */
	virtual void Shutdown()
	{
		KBSMarkerExpiryIdleTask::Shutdown();
		KBSBookScope::ShutdownCleanup();
		KBSResultModel::ShutdownCleanup();
	}
};

/* CREATE_PMINTERFACE
   Binds the C++ implementation class onto its ImplementationID.
*/
CREATE_PMINTERFACE(KBSStartupShutdown, kKBSStartupShutdownImpl)

// End, KBSStartupShutdown.cpp.
