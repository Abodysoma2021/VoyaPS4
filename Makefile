CXX ?= c++
HOST_FLAGS := -std=c++17 -Wall -Wextra -Werror -O2 -Isrc -Ivendor -pthread -DVOYA_VERSION='"$(shell cat VERSION)"'
CORE := src/metadata.cpp src/sha256.cpp src/transfer.cpp src/library.cpp
CURL_FLAGS = $(shell pkg-config --cflags --libs libcurl 2>/dev/null || curl-config --cflags --libs)

.PHONY: test host ps4 preview
host: build/test-driver
build/test-driver: $(CORE) src/platform/http_curl.cpp tests/driver.cpp src/core.hpp src/library.hpp vendor/json.hpp VERSION
	@mkdir -p build
	$(CXX) $(HOST_FLAGS) $(CORE) src/platform/http_curl.cpp tests/driver.cpp $(CURL_FLAGS) -o $@
test: host
	./build/test-driver
	python3 tests/integration.py
ps4:
	bash tools/build-ps4.sh
preview:
	@mkdir -p build
	$(CXX) $(HOST_FLAGS) $(CORE) src/platform/http_curl.cpp src/ui.cpp tools/preview.cpp $(CURL_FLAGS) -o build/preview
	./build/preview
