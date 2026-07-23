//========================================================================================
//
//  Owner: KohakuNekotarou
//
//  KohakuBookSearch (KBS)
//
//  One-shot timer that takes the jump marker back off the screen shortly after it appears.
//  Driven by KBSDrawEventHandler: SetMarker arms it, ClearMarker disarms it. This is the plugin's
//  only idle task - the single justified exception to "avoid idle tasks" (a marker must expire on
//  wall-clock time, and the SDK's only main-thread "call me back in n ms" is an idle task). Ported
//  from KESCL's KESCLMarkerExpiryIdleTask so the proven, robust teardown is kept exactly.
//
//========================================================================================

#ifndef __KBSMarkerExpiryIdleTask_h__
#define __KBSMarkerExpiryIdleTask_h__

/** Jump-marker expiry timer. Only KBSDrawEventHandler should drive this - going through
    SetMarker / ClearMarker keeps the marker state and the timer in step. */
namespace KBSMarkerExpiryIdleTask
{
	/** (Re)start the countdown to clearing the marker. Called every time a marker is shown, so an
	    already-running countdown starts over: each jump shows its marker for the full time. */
	void Start();

	/** Cancel the countdown. Safe to call when it is not running. */
	void Stop();

	/** Release the idle task for good (application shutdown). After this, Start() is a no-op. */
	void Shutdown();
}

#endif // __KBSMarkerExpiryIdleTask_h__
