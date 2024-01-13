#!/usr/bin/env bash

echo "Exporting schematic..."
pdftoppm -r 300 -png -singlefile electronics/schematic/outputs/schematic.pdf electronics/schematic/outputs/schematic

cat > Schematic.md <<EOF
\newpage{}

# Schematic

![Schematic](electronics/schematic/outputs/schematic.png)
EOF

echo "Compiling pdf..."
pandoc -f markdown -t pdf -o manual.pdf README.md sd-card/README.md Schematic.md

echo "Zipping enclosure..."
zip -r enclosure.zip enclosure

echo "Zipping SD-Card..."
pushd sd-card
zip -r ../sd-card.zip *
popd

mkdir -p dist

mv manual.pdf dist/
mv enclosure.zip dist/
mv sd-card.zip dist/
cp electronics/schematic/outputs/schematic.pdf dist/

rm Schematic.md