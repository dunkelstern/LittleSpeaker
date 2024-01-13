{ pkgs ? import <nixpkgs> {} }:

pkgs.mkShell {
  nativeBuildInputs = with pkgs.buildPackages; [
    pandoc
    texlive.combined.scheme-full
    poppler_utils
 ];
}