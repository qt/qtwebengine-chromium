#!/bin/bash

set -o xtrace
set -o errexit
set -o nounset
set -o pipefail

IMAGE_NAME="attested-gemma"
PROJECT_ID="oak-examples-477357"
REPOSITORY_NAME="attested-gemma"
IMAGE_URL="europe-west1-docker.pkg.dev/${PROJECT_ID}/${REPOSITORY_NAME}/${IMAGE_NAME}:latest"

# Build Docker image.
docker build --tag=${IMAGE_URL} .

# Publish Docker image.
docker push ${IMAGE_URL}

echo "Attested Gemma container image is available on ${IMAGE_URL}"
