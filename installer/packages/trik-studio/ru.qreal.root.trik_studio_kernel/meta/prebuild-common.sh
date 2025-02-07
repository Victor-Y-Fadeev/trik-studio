#!/bin/bash
set -o nounset
set -o errexit

cd "$(dirname "$0")"

cp     $INSTALLER_ROOT/licenses/Box2D-license*.txt                      $PWD

mkdir -p $PWD/../data/plugins/editors
mkdir -p $PWD/../data/plugins/tools
cp -r  $BIN_DIR/help                                                    $PWD/../data
cp -r  $BIN_DIR/images                                                  $PWD/../data
cp -r  $BIN_DIR/externalToolsConfig                                     $PWD/../data
cp     $BIN_DIR/splashscreen.png                                        $PWD/../data
cp     $BIN_DIR/system.{js,py}                                               $PWD/../data
