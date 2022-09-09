import argparse


def main() -> None:
    parser = argparse.ArgumentParser(description='Hydrological modelling framework.')
    parser.add_argument('model_name', type=str,
                        help='name of the hydrological model to build.')
    parser.add_argument('--output-dir', type=str, required=True, metavar='DIR',
                        help='output directory to save the results.')

    args = parser.parse_args()

    # do something...


if __name__ == "__main__":
    main()
