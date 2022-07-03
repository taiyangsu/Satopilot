#!/usr/bin/env bash
set -e

export GIT_COMMITTER_NAME="cydia2020"
export GIT_COMMITTER_EMAIL="12470297+cydia2020@users.noreply.github.com"
export GIT_AUTHOR_NAME="cydia2020"
export GIT_AUTHOR_EMAIL="12470297+cydia2020@users.noreply.github.com"

export GIT_SSH_COMMAND="ssh -i /data/gitkey"

# set CLEAN to build outside of CI
if [ ! -z "$CLEAN" ]; then
  # Create folders
  rm -rf /data/openpilot
  mkdir -p /data/openpilot
  cd /data/openpilot

  # Create git repo
  git init
  git remote add origin git@github.com:cydia2020/dodgypilot.git
  git fetch origin devel-staging
else
  cd /data/openpilot
  git clean -xdf
  git branch -D release2-staging || true
fi

git fetch origin release2-staging

# Create release2 with no history
if [ ! -z "$CLEAN" ]; then
  git checkout --orphan release2-staging origin/devel-staging
else
  git checkout --orphan release2-staging
fi

VERSION=$(cat selfdrive/common/version.h | awk -F[\"-]  '{print $2}')
echo "#define COMMA_VERSION \"$VERSION-release\"" > selfdrive/common/version.h

git commit -m "dodgypilot v$VERSION"

# Build signed panda firmware
pushd panda/
CERT=/data/openpilot/panda/certs/debug RELEASE=0 scons -u .
mv board/obj/panda.bin.signed /tmp/panda.bin.signed
popd

# Build stuff
ln -sfn /data/openpilot /data/pythonpath
export PYTHONPATH="/data/openpilot:/data/openpilot/pyextra"
scons -j3

# Ensure no submodules in release
if test "$(git submodule--helper list | wc -l)" -gt "0"; then
  echo "submodules found:"
  git submodule--helper list
  exit 1
fi
git submodule status

# Cleanup
find . -name '*.a' -delete
find . -name '*.o' -delete
find . -name '*.os' -delete
find . -name '*.pyc' -delete
find . -name 'moc_*' -delete
find . -name '__pycache__' -delete
rm -rf panda/board panda/certs panda/crypto
rm -rf .sconsign.dblite Jenkinsfile release/
rm models/supercombo.dlc

# Move back signed panda fw
mkdir -p panda/board/obj
mv /tmp/panda.bin.signed panda/board/obj/panda.bin.signed

# Restore third_party
git checkout third_party/

# Mark as prebuilt release
touch prebuilt

# Add built files to git
git add -f .
git commit --amend -m "openpilot v$VERSION"

# Print committed files that are normally gitignored
#git status --ignored

git remote set-url origin git@github.com:cydia2020/dodgypilot.git

# Push to release2-staging
git push -f origin release2-staging
