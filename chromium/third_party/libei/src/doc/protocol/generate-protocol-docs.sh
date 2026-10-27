#!/usr/bin/env bash
#
set -e

REPO_BASE_DIR="$PWD"

OUTDIR="$PWD"
SITENAME="ei"

# This can be dropped once we update to Fedora 39, the current F38
# version of hugo triggers errors with anything after
# hugo-theme-relearn 1250bf30a8c2e3c1bfac285489bc63f2c395b641
# ERROR 2023/10/11 02:57:59 render of "page" failed: "/root/libei/build/doc/protocol/ei/themes/hugo-theme-relearn/layouts/_default/single.html:1:4": execute of template failed: template: _default/single.html:1:4: executing "_default/single.html" at <partial "_main.hugo" .>: error calling partial: "/root/libei/build/doc/protocol/ei/themes/hugo-theme-relearn/layouts/partials/_main.hugo:1:4": execute of template failed: template: partials/_main.hugo:1:4: executing "partials/_main.hugo" at <partialCached "page-meta.hugo" . .RelPermalink>: error calling partialCached: "/root/libei/build/doc/protocol/ei/themes/hugo-theme-relearn/layouts/partials/page-meta.hugo:11:123": execute of template failed: template: partials/page-meta.hugo:11:123: executing "partials/page-meta.hugo" at <index .Site.Params $disable>: error calling index: index of untyped nil
TEMPLATE_SHA=aa0f4089cb9c7691e7dc3aff003dddcde1ba02f4

while [[ $# -gt 0 ]]; do
  case "$1" in
    --verbose | -v)
      set -x
      shift
      ;;
    --scanner)
      SCANNER="$2"
      shift 2;
      ;;
    --protocol)
      PROTOFILE="$2"
      shift 2
      ;;
    --template-dir)
      TEMPLATEDIR="$2"
      shift 2
      ;;
    --output-dir)
      OUTDIR="$2"
      shift 2
      ;;
    --git-repo)
      REPO_BASE_DIR="$2"
      shift 2;
      ;;
    **)
      echo "Unknown argument: $1"
      exit 1
      ;;
  esac
done

if [[ -z "$SCANNER" ]]; then
  SCANNER="$REPO_BASE_DIR/proto/ei-scanner"
fi

if [[ -z "$PROTOFILE" ]]; then
  PROTOFILE="$REPO_BASE_DIR/proto/protocol.xml"
fi

if [[ -z "$TEMPLATEDIR" ]]; then
  TEMPLATEDIR="$REPO_BASE_DIR/doc/protocol/"
fi

SITEDIR="$OUTDIR/$SITENAME"
if [[ -e "$SITEDIR" ]]; then
  echo "$SITEDIR already exists, updating"
else
  hugo new site "$SITEDIR"
  git clone https://github.com/McShelby/hugo-theme-relearn "$SITEDIR/themes/hugo-theme-relearn"
  pushd "$SITEDIR/themes/hugo-theme-relearn" > /dev/null || exit 1
  git reset --hard $TEMPLATE_SHA
  popd > /dev/null || exit 1
fi

cp "$TEMPLATEDIR/config.toml" "$SITEDIR/"

pushd "$TEMPLATEDIR" > /dev/null || exit 1
find . -type f -name "*.md"
find . -type f -name "*.md" -exec install -D {} "$SITEDIR/content/{}" \;
popd > /dev/null || exit 1

pushd "$SITEDIR" > /dev/null || exit 1

# Generate a list of available interfaces and read that
# list line-by-line to generate a separate .md file
# for each interface
while read -r iface; do
  $SCANNER --component=ei --jinja-extra-data="{ \"interface\": \"$iface\" }" --output="$SITEDIR/content/interfaces/$iface.md" "$PROTOFILE" "$TEMPLATEDIR/interface.md.tmpl"
done < <($SCANNER --component=ei "$PROTOFILE" - <<EOF
{% for interface in interfaces %}
{{interface.name}}
{% endfor %}
EOF
)

hugo

popd > /dev/null || exit 1
