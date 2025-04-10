{
  flake,
  inputs,
  pkgs,
  ...
}:
# treefmt with config
let
  formatter = inputs.treefmt-nix.lib.mkWrapper pkgs {
    _file = __curPos.file;
    imports = [ ./treefmt.nix ];
  };

  check =
    pkgs.runCommand "format-check"
      {
        nativeBuildInputs = [
          formatter
          pkgs.git
        ];

        meta.platforms = with pkgs.lib.platforms; [
          linux
          darwin
        ];
      }
      ''
        export HOME=$NIX_BUILD_TOP/nixos-config

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
      check = check;
    };
  };
}
