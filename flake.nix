{
  description = "QT6 C++ Development Environment";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
  };

  outputs =
    { self, nixpkgs }:
    let
      supportedSystems = [
        "x86_64-linux"
        "aarch64-linux"
        "x86_64-darwin"
        "aarch64-darwin"
      ];

      forAllSystems = nixpkgs.lib.genAttrs supportedSystems;
      nixpkgsFor = forAllSystems (system: import nixpkgs { inherit system; });
    in
    {
      # 1. Build rules for "nix build"
      packages = forAllSystems (
        system:
        let
          pkgs = nixpkgsFor.${system};
        in
        {
          default = pkgs.stdenv.mkDerivation {
            pname = "image2pdfs-qt";
            version = "0.1.0";

            # Use cleanSource to avoid copying .git or ./build into the nix store
            src = pkgs.lib.cleanSource ./.;

            # Build-time dependencies
            nativeBuildInputs = with pkgs; [
              cmake
              ninja
              pkg-config
              qt6.wrapQtAppsHook # Essential for fixing Qt environment paths
            ];

            # Runtime dependencies (Libraries)
            buildInputs = (with pkgs.qt6; [
              qtbase
              qtdeclarative
              qtsvg
              qttools
              qt5compat
            ])
            ++ (if pkgs.stdenv.isLinux then [ pkgs.qt6.qtwayland ] else []);
          };
        }
      );

      # 2. Development environment for "nix develop"
      devShells = forAllSystems (
        system:
        let
          pkgs = nixpkgsFor.${system};
        in
        {
          default = pkgs.mkShell {
            name = "qt6-dev-shell";

            # Inherit dependencies from the package definition above
            # This ensures dev env matches build env exactly
            inputsFrom = [ self.packages.${system}.default ];

            packages = with pkgs; [
              gdb
              clang-tools # clangd, clang-format
            ];

            CMAKE_EXPORT_COMPILE_COMMANDS = "1";

            shellHook = ''
              echo "Qt Version: $(qmake --version | grep -o 'Qt version [0-9.]*' | cut -d ' ' -f 3)"
              echo "Compiler: $(c++ --version | head -n 1)"
              ln -sf build/compile_commands.json .
            '';
          };
        }
      );
    };
}
