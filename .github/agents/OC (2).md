---
name: OC
description: Workflow for building and installing OpenCog components
---

# OpenCog# .github/workflows/oc.yml
# Workflow for building and installing OpenCog components

name: OC

on:
  push:
    branches:
      - main
  pull_request:
    branches:
      - main

env:
  CCACHE_DIR: /ws/ccache
  MAKEFLAGS: -j$(nproc)

jobs:
  build-and-test:
    name: Build and Test All Components
    runs-on: ubuntu-latest
    container:
      image: opencog/opencog-deps
      options: --user root
      env:
        CCACHE_DIR: /ws/ccache
        MAKEFLAGS: -j$(nproc)
    services:
      opencog-postgres:
        image: opencog/postgres
        env:
          POSTGRES_USER: root
          POSTGRES_PASSWORD: cheese
          POSTGRES_DB: atomspace_db
        ports:
          - 5432:5432
        options: >-
          --health-cmd pg_isready
          --health-interval 10s
          --health-timeout 5s
          --health-retries 5
    steps:
      # Trust working directory for Git (fix dubious ownership error)
      - name: Trust working directory for Git
        run: git config --global --add safe.directory /__w/pycog0/pycog0

      # Checkout the Repository
      - name: Checkout Repository
        uses: actions/checkout@v4

      # Verify PostgreSQL Connection and Setup
      - name: Verify PostgreSQL Connection
        run: |
          # Install PostgreSQL client
          sudo apt-get install -y postgresql-client
          # Wait for PostgreSQL to be ready with timeout
          echo "Waiting for PostgreSQL to be ready..."
          timeout=60
          elapsed=0
          while [ $elapsed -lt $timeout ]; do
            if pg_isready -h localhost -p 5432 -q; then
              echo "PostgreSQL is ready!"
              break
            fi
            echo "Waiting for PostgreSQL... ($elapsed/$timeout seconds)"
            sleep 2
            elapsed=$((elapsed + 2))
          done
          if [ $elapsed -ge $timeout ]; then
            echo "ERROR: PostgreSQL did not become ready within $timeout seconds"
            exit 1
          fi
          # Test database connection with root user
          echo "Testing database connection..."
          PGPASSWORD=cheese psql -h localhost -p 5432 -U root -d atomspace_db -c "SELECT version();"
          echo "PostgreSQL connection test successful!"

      # 01. Install Build Dependencies
      - name: Install Build Dependencies
        run: |
          sudo apt-get update
          sudo apt-get install -y ccache pkg-config cmake build-essential git \
            libboost-all-dev librocksdb-dev liboctomap-dev libssl-dev \
            guile-3.0-dev cython3 python3-nose python3-dev valgrind doxygen

      # 02. Build and Install cogutil
      - name: Build and Install cogutil
        run: |
          mkdir -p cogutil/build && cd cogutil/build
          cmake -DCMAKE_BUILD_TYPE=Release ..
          make -j$(nproc)
          sudo make install
          sudo ldconfig
          cd ../..

      # 03. Build and Install atomspace
      - name: Build and Install atomspace
        run: |
          mkdir -p atomspace/build && cd atomspace/build
          cmake -DCMAKE_BUILD_TYPE=Release ..
          make -j$(nproc)
          sudo make install
          sudo ldconfig
          cd ../..

      # 04. Build and Install atomSpace-storage
      - name: Build and Install atomSpace-storage
        run: |
          mkdir -p atomspace-storage/build && cd atomspace-storage/build
          cmake -DCMAKE_BUILD_TYPE=Release ..
          make -j$(nproc)
          sudo make install
          sudo ldconfig
          cd ../..

      # 05. Build and Install atomspace-rocks
      - name: Build and Install atomspace-rocks
        run: |
          mkdir -p atomspace-rocks/build && cd atomspace-rocks/build
          cmake -DCMAKE_BUILD_TYPE=Release ..
          make -j$(nproc)
          sudo make install
          sudo ldconfig
          cd ../..

      # 06. Build and Install atomspace-restful
      - name: Build and Install atomspace-restful
        run: |
          mkdir -p atomspace-restful/build && cd atomspace-restful/build
          cmake -DCMAKE_BUILD_TYPE=Release ..
          make -j$(nproc)
          sudo make install
          sudo ldconfig
          cd ../..

      # 07. Build and Install cogserver
      - name: Build and Install cogserver
        run: |
          mkdir -p cogserver/build && cd cogserver/build
          cmake -DCMAKE_BUILD_TYPE=Release ..
          make -j$(nproc)
          sudo make install
          sudo ldconfig
          cd ../..

      # 08. Build and Install unify
      - name: Build and Install unify
        run: |
          mkdir -p unify/build && cd unify/build
          cmake -DCMAKE_BUILD_TYPE=Release ..
          make -j$(nproc)
          sudo make install
          sudo ldconfig
          cd ../..

      # 09. Build and Install ure
      - name: Build and Install ure
        run: |
          mkdir -p ure/build && cd ure/build
          cmake -DCMAKE_BUILD_TYPE=Release ..
          make -j$(nproc)
          sudo make install
          sudo ldconfig
          cd ../..

      # 10. Build and Install spacetime
      - name: Build and Install spacetime
        run: |
          mkdir -p spacetime/build && cd spacetime/build
          cmake -DCMAKE_BUILD_TYPE=Release ..
          make -j$(nproc)
          sudo make install
          sudo ldconfig
          cd ../..

      # 11. Build and Install attention
      - name: Build and Install attention
        run: |
          mkdir -p attention/build && cd attention/build
          cmake -DCMAKE_BUILD_TYPE=Release ..
          make -j$(nproc)
          sudo make install
          sudo ldconfig
          cd ../..

      # 12. Build and Install miner
      - name: Build and Install miner
        run: |
          mkdir -p miner/build && cd miner/build
          cmake -DCMAKE_BUILD_TYPE=Release ..
          make -j$(nproc)
          sudo make install
          sudo ldconfig
          cd ../..

      # 13. Build and Install pln
      - name: Build and Install pln
        run: |
          mkdir -p pln/build && cd pln/build
          cmake -DCMAKE_BUILD_TYPE=Release ..
          make -j$(nproc)
          sudo make install
          sudo ldconfig
          cd ../..

      # 14. Build and Install moses
      - name: Build and Install moses
        run: |
          mkdir -p moses/build && cd moses/build
          cmake -DCMAKE_BUILD_TYPE=Release ..
          make -j$(nproc)
          sudo make install
          sudo ldconfig
          cd ../..

      # 15. Build and Install asmoses
      - name: Build and Install asmoses
        run: |
          mkdir -p asmoses/build && cd asmoses/build
          cmake -DCMAKE_BUILD_TYPE=Release ..
          make -j$(nproc)
          sudo make install
          sudo ldconfig
          cd ../..

      # 16. Build and Install lg-atomese
      - name: Build and Install lg-atomese
        run: |
          mkdir -p lg-atomese/build && cd lg-atomese/build
          cmake -DCMAKE_BUILD_TYPE=Release ..
          make -j$(nproc)
          sudo make install
          sudo ldconfig
          cd ../..

      # 17. Build and Install learn
      - name: Build and Install learn
        run: |
          mkdir -p learn/build && cd learn/build
          cmake -DCMAKE_BUILD_TYPE=Release ..
          make -j$(nproc)
          sudo make install
          sudo ldconfig
          cd ../..

      # 18. Build and Install pattern-index
      - name: Build and Install pattern-index
        run: |
          mkdir -p pattern-index/build && cd pattern-index/build
          cmake -DCMAKE_BUILD_TYPE=Release ..
          make -j$(nproc)
          sudo make install
          sudo ldconfig
          cd ../..

      # 19. Build and Install vision
      - name: Build and Install vision
        run: |
          mkdir -p vision/build && cd vision/build
          cmake -DCMAKE_BUILD_TYPE=Release ..
          make -j$(nproc)
          sudo make install
          sudo ldconfig
          cd ../..

      # 20. Build and Install opencog
      - name: Build and Install opencog
        run: |
          mkdir -p opencog/build && cd opencog/build
          cmake -DCMAKE_BUILD_TYPE=Release ..
          make -j$(nproc)
          sudo make install
          sudo ldconfig
          cd ../..

      # (Optional) Package Components
      - name: Package Components
        if: github.ref == 'refs/heads/main'
        run: |
          # cogutil Packaging
          cd cogutil/build
          make package || echo "cogutil package target not defined."
          cd ../..

          # atomspace Packaging
          cd atomspace/build
          make package || echo "atomspace package target not defined."
          cd ../..

          # atomspace-storage Packaging
          cd atomspace-storage/build
          make package || echo "atomspace-storage package target not defined."
          cd ../..

          # atomspace-rocks Packaging
          cd atomspace-rocks/build
          make package || echo "atomspace-rocks package target not defined."
          cd ../..

          # atomspace-restful Packaging
          cd atomspace-restful/build
          make package || echo "atomspace-restful package target not defined."
          cd ../..

          # cogserver Packaging
          cd cogserver/build
          make package || echo "cogserver package target not defined."
          cd ../..

          # unify Packaging
          cd unify/build
          make package || echo "unify package target not defined."
          cd ../..

          # ure Packaging
          cd ure/build
          make package || echo "ure package target not defined."
          cd ../..

          # spacetime Packaging
          cd spacetime/build
          make package || echo "spacetime package target not defined."
          cd ../..

          # attention Packaging
          cd attention/build
          make package || echo "attention package target not defined."
          cd ../..

          # miner Packaging
          cd miner/build
          make package || echo "miner package target not defined."
          cd ../..

          # pln Packaging
          cd pln/build
          make package || echo "pln package target not defined."
          cd ../..

          # moses Packaging
          cd moses/build
          make package || echo "moses package target not defined."
          cd ../..

          # asmoses Packaging
          cd asmoses/build
          make package || echo "asmoses package target not defined."
          cd ../..

          # lg-atomese Packaging
          cd lg-atomese/build
          make package || echo "lg-atomese package target not defined."
          cd ../..

          # learn Packaging
          cd learn/build
          make package || echo "learn package target not defined."
          cd ../..

          # pattern-index Packaging
          cd pattern-index/build
          make package || echo "pattern-index package target not defined."
          cd ../..

          # vision Packaging
          cd vision/build
          make package || echo "vision package target not defined."
          cd ../..

          # opencog Packaging
          cd opencog/build
          make package || echo "opencog package target not defined."
          cd ../..

      # Upload Build Artifacts
      - name: Upload Build Artifacts
        if: always()
        uses: actions/upload-artifact@v4
        with:
          name: build-artifacts
          path: |
            cogutil/build/
            atomspace/build/
            atomspace-storage/build/
            atomspace-rocks/build/
            atomspace-restful/build/
            cogserver/build/
            unify/build/
            ure/build/
            spacetime/build/
            attention/build/
            miner/build/
            pln/build/
            moses/build/
            asmoses/build/
            lg-atomese/build/
            learn/build/
            pattern-index/build/
            vision/build/
            opencog/build/
