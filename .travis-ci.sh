PACKAGE=jet

echo "yes" | sudo add-apt-repository 'deb http://llvm.org/apt/precise/ llvm-toolchain-precise-3.7 main'
wget -O - http://llvm.org/apt/llvm-snapshot.gpg.key | sudo apt-key add -

sudo apt-get update -qq
sudo apt-get install -qq -y llvm-3.7 clang-3.7

