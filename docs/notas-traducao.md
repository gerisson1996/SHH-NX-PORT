# Notes from the translation of `default.xex`

- Game: Title ID `4B4E07E5`, "Build v18.51" (Code Changelist #437812), XEX file time 2008-08-14.
- The first codegen failed on two branch targets with no function; they are declared in `app/overrides.toml`.
  The second run wrote 152 C++ files (about 163 MB) in about 2 minutes.
- `sdk/` comes from nfsmw-nx and had some NFSMW tuning, for example XMA audio and file reads.
- The first run stopped at once on `Call to invalid or unregistered function at guest address 0x8268C720`: a code
  gap the analysis left without a function. `tools/huecos.py --min 8` found 908 gaps. Declaring them all split some
  real functions (356 branch warnings in the codegen log) and some were data or import thunks.
  `tools/huecos_conflictos.py` removes those from `app/huecos.toml` and lists them in `tools/huecos_excluir.txt`.
  After two passes, 757 gaps remain and the codegen gives no warnings.
- The emulated Xenos GPU was too slow on the Switch; the game draws through Renut (Direct3D 9 on deko3d), from the
  sa2-nx and banjo-nx ports.
- CPU: the code generator's register-as-local options (`app/overrides.toml`, top) and `setjmp`/`longjmp`
  (`0x8292CE20` / `0x8292CB00`). `non_volatile_as_local` stays off: with it the game crashed in `sub_8240AAD0`.
- GPU at 720p handheld: see `docs/perfil-shh.md` (single sample, shadows at 2/3, bound-fetch types and point-light
  gating as specialization constants, the scene's two-half resolve joined).
