import csv
import random

USERNAMES = ["testuser", "testuser2"]
SIDES = ["buy", "sell"]
ORDER_TYPES = ["limit", "market"]


def generate_fuzz_csv(file_path: str, num_lines: int):
    with open(file_path, 'w', newline='') as csvfile:
        writer = csv.writer(csvfile)
        writer.writerow(
            ["action", "username", "side", "qty", "order_type", "price"]
        )
        writer.writerow(["login", "testuser", "", "", "", ""])
        writer.writerow(["login", "testuser2", "", "", "", ""])

        for i in range(num_lines):
            username = random.choice(USERNAMES)

            side = random.choice(SIDES)
            order_type = random.choice(ORDER_TYPES)
            if order_type == "limit":
                price = round(random.normalvariate(100, 0.5), 4)
                qty = round(random.uniform(1, 1000), 2)
            else:
                price = ""
                qty = round(random.uniform(1, 300), 2)

            row = ["order", username, side, qty, order_type, price]

            writer.writerow(row)
            print(f"Generated Line {i + 1}/{num_lines}", end="\r")

        writer.writerow(["login", "testuser", "", "", "", ""])
        writer.writerow(["login", "testuser2", "", "", "", ""])
        print(f"File written to {file_path}")


def main(file_path: str, num_lines: int):
    generate_fuzz_csv(file_path=file_path, num_lines=num_lines)


if __name__ == "__main__":
    file_path = "tests/bulk_testing/csvs/fuzz_test.csv"
    n = 10_000_000
    main(file_path, n)
