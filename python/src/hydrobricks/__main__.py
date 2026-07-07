"""Allow running the CLI as ``python -m hydrobricks``."""

import sys

from hydrobricks.cli import main

if __name__ == "__main__":
    sys.exit(main())
