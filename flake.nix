{
  description = "The view-module backend — plugin glue and CMake for Logos view (ui_qml) modules";

  # A LEAF by design. This repo emits the Qt plugin around a view's .rep and
  # *Backend, and builds the replica factory for the same .rep — neither of
  # which needs to read a LIDL contract, so nothing here depends on a generator
  # frontend, an SDK, or the protocol. Keeping it that way is what lets it be
  # re-pinned independently of the SDK stack.
  inputs = {
    logos-nix.url = "github:logos-co/logos-nix";
    nixpkgs.follows = "logos-nix/nixpkgs";
  };

  outputs = { self, nixpkgs, ... }:
    let
      systems = [ "aarch64-darwin" "x86_64-darwin" "aarch64-linux" "x86_64-linux" ];

      forAllSystems = f: nixpkgs.lib.genAttrs systems (system: f {
        inherit system;
        pkgs = import nixpkgs { inherit system; };
      });

      mkGenerator = pkgs: pkgs.stdenv.mkDerivation {
        pname = "logos-view-generator";
        version = "0.1.0";
        src = ./view-generator;
        nativeBuildInputs = [ pkgs.cmake pkgs.qt6.wrapQtAppsNoGuiHook ];
        buildInputs = [ pkgs.qt6.qtbase ];
      };

      # The CMake half: LogosViewModule.cmake plus the four .in templates it
      # resolves as siblings. They MUST stay in one directory — the function
      # locates them through CMAKE_CURRENT_FUNCTION_LIST_DIR.
      mkCmakeModule = pkgs: pkgs.runCommand "logos-view-module-cmake" {} ''
        mkdir -p $out/share/cmake/LogosViewModule
        cp ${./cmake}/LogosViewModule.cmake        $out/share/cmake/LogosViewModule/
        cp ${./cmake}/LogosViewPluginBase.h.in     $out/share/cmake/LogosViewModule/
        cp ${./cmake}/LogosViewPluginBase.cpp.in   $out/share/cmake/LogosViewModule/
        cp ${./cmake}/LogosViewReplicaFactory.h.in $out/share/cmake/LogosViewModule/
        cp ${./cmake}/LogosViewReplicaFactory.cpp.in $out/share/cmake/LogosViewModule/
      '';

      # The header a view's *Backend derives, alongside its repc SimpleSource.
      mkInclude = pkgs: pkgs.runCommand "logos-view-module-include" {} ''
        mkdir -p $out/include
        cp ${./cpp}/logos_ui_plugin_context.h $out/include/
      '';

    in {
      packages = forAllSystems ({ pkgs, ... }: rec {
        logos-view-generator = mkGenerator pkgs;
        cmake-module = mkCmakeModule pkgs;
        include = mkInclude pkgs;
        logos-view-module = pkgs.symlinkJoin {
          name = "logos-view-module";
          paths = [ logos-view-generator cmake-module include ];
        };
        default = logos-view-module;
      });

      checks = forAllSystems ({ pkgs, system, ... }: {
        # Drive the generator over a real .rep + metadata.json and assert it
        # emits the three files with the class names scraped from them. Cheap,
        # and it is the only thing that catches a .rep parse regression.
        view-generator = pkgs.runCommand "logos-view-generator-test" {
          nativeBuildInputs = [ self.packages.${system}.logos-view-generator ];
        } ''
          mkdir -p work && cd work
          cat > metadata.json <<'EOF'
          { "name": "ticker_panel", "version": "2.1.0", "type": "ui_qml" }
          EOF
          cat > Ticker.rep <<'EOF'
          class TickerBackend
          {
              SLOT(void refresh());
              PROP(QString symbol READWRITE);
          };
          EOF
          logos-view-generator --metadata metadata.json --rep Ticker.rep --output-dir out

          for f in ticker_panel_ui_interface.h ticker_panel_ui_glue.h ticker_panel_ui_glue.cpp; do
            test -f "out/$f" || { echo "MISSING: $f"; exit 1; }
          done
          # The class stem is PascalCase of the module name, and the rep class
          # is scraped from the .rep — both are what a wrong parse gets wrong.
          grep -q "TickerPanel" out/ticker_panel_ui_glue.h \
            || { echo "plugin base class not derived from module name"; exit 1; }
          grep -q "TickerBackend" out/ticker_panel_ui_glue.cpp \
            || { echo "rep class not scraped from the .rep"; exit 1; }
          # version() is emitted inline in the glue HEADER, not the .cpp.
          grep -q 'version() const override.*"2.1.0"' out/ticker_panel_ui_glue.h \
            || { echo "version not carried from metadata.json"; exit 1; }
          touch $out
        '';

        # A missing/!unparseable .rep must FAIL, not emit half a plugin.
        view-generator-rejects-bad-rep = pkgs.runCommand "logos-view-generator-reject-test" {
          nativeBuildInputs = [ self.packages.${system}.logos-view-generator ];
        } ''
          mkdir -p work && cd work
          echo '{ "name": "broken", "version": "1.0.0" }' > metadata.json
          echo 'this file declares no class' > Bad.rep
          if logos-view-generator --metadata metadata.json --rep Bad.rep --output-dir out; then
            echo "generator accepted a .rep with no class"; exit 1
          fi
          touch $out
        '';
      });

      devShells = forAllSystems ({ pkgs, ... }: {
        default = pkgs.mkShell {
          packages = [ pkgs.cmake pkgs.ninja pkgs.qt6.qtbase pkgs.qt6.qtremoteobjects ];
        };
      });
    };
}
