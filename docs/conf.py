# Sphinx configuration file

import os

BUILD_DIR = os.environ["BUILDDIR"]

# Project Information
project = "MeshNOW"
copyright = "2023, Marvin Bauer"
author = "Marvin Bauer"
release = "1.0"
language = "en"

# Extensions
extensions = ["breathe"]


# General configuration
exclude_patterns = ["_build", "Thumbs.db", ".DS_Store"]

# Doxygen support
breathe_projects = {"MeshNOW": os.path.join(BUILD_DIR, "xml")}
breathe_default_project = "MeshNOW"

# HTML specific
html_theme = "sphinx_rtd_theme"
html_static_path = ["_static"]
html_css_files = ["custom.css"]
