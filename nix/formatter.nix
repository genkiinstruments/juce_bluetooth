{ flake
, inputs
, pkgs
, ...
}:
let
  treefmt = {
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
    programs.actionlint.enable = true;

    settings.formatter.clang-format.options = [ "-style=file:${flake}/.clang-format" ];

    settings.formatter.prettier.includes = [
      "*.yaml"
      "*.yml"
    ];
  };
  treefmtEval = inputs.treefmt-nix.lib.evalModule pkgs treefmt;
  formatter = treefmtEval.config.build.wrapper;
  check = treefmtEval.config.build.check flake;
in
formatter
#   // {
#   meta = formatter.meta // {
#     tests = {
#       inherit check;
#     };
#   };
# }
