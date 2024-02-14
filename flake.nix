{
  description = "env flake";

  inputs.nixpkgs.url = "github:nixos/nixpkgs/nixpkgs-unstable";
  inputs.flake-utils.url = "github:numtide/flake-utils";

  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem (system: let
      pkgs = nixpkgs.legacyPackages.${system};


      tgbot-cpp = with pkgs; stdenv.mkDerivation rec {
        name = "tgbot-cpp";
        src = fetchFromGitHub {
          owner = "7FM";
          repo = name;
          rev = "06153f60377705c49b49cd56f5cf23c91d8347bd";
          sha256 = "sha256-uTtfyriONQC+oXhwG2q7nCnsemf2mYZx5+1HSallsgo=";
          # owner = "reo7sp";
          # rev = "f1c2dbad1c9674f82e61b7cf76cdc618a2dba852";
          # sha256 = "sha256-/oCpSS+FjVzaTjM8Zov0FEBeEJCzJpqXHV8xUFQvIEk=";
        };

        nativeBuildInputs = [
          cmake
          boost
          zlib
          openssl
        ];
      };

      drynomore-telegram-bot = with pkgs; stdenv.mkDerivation {
          pname = "drynomore-telegram-bot";
          version = "0.0.1";
          src = ./server/telegram_bot;
          nativeBuildInputs = [
            cmake
            libyamlcpp
            tgbot-cpp
            boost
            zlib
            openssl
          ];
        };

    in rec {
      defaultPackage = drynomore-telegram-bot;
      devShell = pkgs.mkShell {
        buildInputs = [
          drynomore-telegram-bot
        ];
        nativeBuildInputs = with pkgs; [
          gdb
          gnumake
          cmake
          openssl
          boost
          zlib
          libyamlcpp
        ];
      };
    }
  );
}
