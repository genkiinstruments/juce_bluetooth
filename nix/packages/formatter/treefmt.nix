_:
let
  excludes = [
    "extern/**/*"
    "common/eurorack/**/*"
    "build*/**/*"
  ];
in
{
  projectRootFile = "flake.nix";

  programs.nixpkgs-fmt.enable = true;
  programs.deadnix.enable = true;
  programs.statix.enable = true;
  programs.prettier.enable = true;
  programs.clang-format.enable = true;
  programs.shellcheck.enable = true;
  programs.shfmt.enable = true;
  programs.ruff.format = true;
  programs.ruff.check = true;

  settings.formatter.clang-format.excludes = excludes;
  settings.formatter.clang-format.options = [ "-style=file:../../../.clang-format" ];

  settings.formatter.ruff-check.excludes = excludes;
  settings.formatter.ruff-format.excludes = excludes;

  settings.formatter.prettier.includes = [
    "*.yaml"
    "*.yml"
  ];
}
