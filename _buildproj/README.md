# _buildproj — build files backup

The real build files live in the InDesign SDK build tree at `build/win/prj/`, which is **outside
this repository**. This folder keeps a copy of them so the project's build customizations are not
lost when only the source is pushed. `build/win/prj/` is authoritative; this is the backup — after
editing a build file there, copy it back here and commit.

## Files kept here
- `KohakuBookSearch.vcxproj` — MSVC project (source file list, linked libs, ProjectGuid).
- `KohakuBookSearch.vcxproj.filters` — Solution Explorer filters.
- `KohakuBookSearchCPP.rsp` — C++ compile response file (extra `/I` include paths).
- `KohakuBookSearchODFRC.rsp` — ODFRC (resource compiler) response file.

## Restore into a clean SDK checkout
1. Copy the four files above into `build/win/prj/`.
2. Copy the plugin sources (this repo's `.cpp` / `.h` / `.fr` / `.rc`) into
   `source/sdksamples/KohakuBookSearch/` (i.e. `source/sdksamples/KBS/`).
3. Register the project in `build/win/prj/SDKSamples.sln`:
   - a `Project(...) = "KohakuBookSearch", "KohakuBookSearch.vcxproj", "{DD125A1E-99DD-4D59-B001-DEB4D37D34C5}"` / `EndProject` block, and
   - eight `{DD125A1E-99DD-4D59-B001-DEB4D37D34C5}.<Debug|Release>|<x64|x86>.<ActiveCfg|Build.0>` lines in `GlobalSection(ProjectConfigurationPlatforms)`.
   The matching `<ProjectGuid>{DD125A1E-99DD-4D59-B001-DEB4D37D34C5}</ProjectGuid>` is already in the .vcxproj.

## Build customizations captured here (do not lose)
- **`DV_WidgetBin.lib`** is linked in all four configs (`AdditionalDependencies`) — needed by the
  self-drawing result cell (`DVControlView` / `StringUtils` / `CTreeViewWidgetMgr` / `kInvalidNodeID`).
- **`KohakuBookSearchCPP.rsp`** adds `/I ..\..\..\source\open\includes\widgets` — `DrawStringUtils.h`
  transitively includes `DVPublicUtilities.h`, which lives there.
- `common/SDKFileHelper.cpp` is compiled in (SDKFileHelper link resolution).
- `WindowsTargetPlatformVersion` is `10.0`.

Build: Release|x64, with InDesign closed (an open InDesign locks the .pln -> LNK1104).
