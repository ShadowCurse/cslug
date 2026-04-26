{ pkgs ? import <nixpkgs> {} }:
pkgs.mkShell {
  LD_LIBRARY_PATH = "${pkgs.lib.makeLibraryPath [pkgs.wayland pkgs.libxkbcommon]}";

  buildInputs = with pkgs; [
    pkg-config
    # raylib

    libGL
    wayland
    wayland-scanner
    libxkbcommon
  ];
}
