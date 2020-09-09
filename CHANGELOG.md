# Changelog
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]
## [2.1.0]
### Added 
- SparseStore: A sparse multi-file backing store interface included [Details](https://llnl-umap.readthedocs.io/en/latest/sparse_store.html)
- Build Option: -DENABLE_DISPLAY_STATS: option to display statistics before uunmap returns [Details](https://llnl-umap.readthedocs.io/en/latest/advanced_configuration.html)
- Build Option: -DENABLE_TESTS_LINK_STATIC_UMAP: This allows test binaries to link Umap statically or dynamically [Details](https://llnl-umap.readthedocs.io/en/latest/advanced_configuration.html)
- Flush Buffer: umap_flush() allows users to explicitly flush dirty pages [#2b36757]

### Fixed
- Fixed C-compatibility of umap.h [#c489653]
- Duplicate stats removed [#3b59db9]
- deadlock in evict_region [#aeb4e6b]
- Thread-safe umap_ex() and uunmap() API calls ([#13863a6],[#12a740b])

## [2.0.0]

[Unreleased]: https://github.com/LLNL/umap/tree/develop
[2.1.0]: https://github.com/LLNL/umap/tree/release-test
[2.0.0]: https://github.com/LLNL/umap/tree/release-2.0.0
[#13863a6]: https://github.com/LLNL/umap/commit/13863a60121072660bb459d03f1a185b082a37c8
[#aeb4e6b]: https://github.com/LLNL/umap/commit/86fd5937aeb4e6bb73dd689a6975cb225a4b3725
[#12a740b]: https://github.com/LLNL/umap/commit/12a740bd85ddd57d50a00071644d4d654176375c
[#c489653]: https://github.com/LLNL/umap/commit/c4896535eacd9266a0c00744f9c14c389a3c89a0
[#3b59db9]: https://github.com/LLNL/umap/commit/3b59db93d29bca092e702ea93f9712e8cc5f147f
[#2b36757]: https://github.com/LLNL/umap/commit/2b36757ae5f83836dafc63702b12bda480f17540

