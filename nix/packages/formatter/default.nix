{ flake
, inputs
, pkgs
, ...
}:
let
  formatter = inputs.treefmt-nix.lib.mkWrapper pkgs {
    _file = __curPos.file;

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

  # [TODO]: Fix `nix flake check` (April 10, 2025 12:30, )
  check =
    pkgs.runCommand "format-check"
      {
        nativeBuildInputs = [
          formatter
          pkgs.git
        ];

        # only check on Linux
        meta.platforms = pkgs.lib.platforms.linux;
      }
      ''
        export HOME=$NIX_BUILD_TOP/genkiinstruments/juce_bluetooth

        # keep timestamps so that treefmt is able to detect mtime changes
        cp --no-preserve=mode --preserve=timestamps -r ${flake} source
        cd source
        git init --quiet
        git add .
        treefmt --no-cache
        if ! git diff --exit-code; then
          echo "-------------------------------"
          echo "aborting due to above changes ^"
          exit 1
        fi
        touch $out
      '';
in
formatter
  // {
  meta = formatter.meta // {
    tests = {
      inherit check;
    };
  };
}
