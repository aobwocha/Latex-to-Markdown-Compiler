set -e

make clean > /dev/null 2>&1 || true
rm -rf dist

if [ ! -d "node_modules" ]; then
  npm install
fi

make

if [ ! -f "output/latex_compiler" ]; then
  echo "Build failed: output/latex_compiler missing"
  exit 1
fi

npm run dev