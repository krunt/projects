#include "fuzzy.h"

double approx_equal(double lhs, double rhs) {
    return abs(lhs - rhs) < 1e-9;
}

class Player {
public:
    Player(double v, double a) 
        : velocity_(v), acceleration_(a)
    {}

    double velocity() const { return velocity_; }
    double acceleration() const { return acceleration_; }

private:
    double velocity_;
    double acceleration_;
};


struct TestCase {
    double vel;
    double accel;
    const char *str;
    double val;
};

TestCase tests[] = {
    /* test #0 */
    {
        15,
        20,
        "\
switch (velocity) {\
    case 10: return 100\
    case 20: return 200\
    default: return 300\
        }",   
        150,
    },

    /* test #1 */
    {
        15,
        15,
        "\
switch (velocity) {\
    case 10: return 100\
    case 20: \
        switch (acceleration) {\
            case 10: return 100\
            case 20: return 200\
            default: return 300\
        }\
    default: return 300\
        }",   
        125,
    },

    /* test #2 */
    {
        15,
        15,
        "\
switch (velocity) {\
    case 10: \
        switch (acceleration) {\
            case 10: return 100\
            case 20: return 300\
            default: return 400\
        }\
    case 20: \
        switch (acceleration) {\
            case 10: return 100\
            case 20: return 200\
            default: return 300\
        }\
    default: return 300\
        }",   
        175,
    },

    /* test #3 */
    {
        25,
        15,
        "\
switch (velocity) {\
    case 10: \
        switch (acceleration) {\
            case 10: return 100\
            case 20: return 300\
            default: return 400\
        }\
    case 20: \
        switch (acceleration) {\
            case 10: return 100\
            case 20: return 200\
            default: return 300\
        }\
    default: return 300\
        }",   
        300,
    },

    /* test #4 */
    {
        5,
        15,
        "\
switch (velocity) {\
    case 10: \
        switch (acceleration) {\
            case 10: return 100\
            case 20: return 300\
            default: return 400\
        }\
    case 20: \
        switch (acceleration) {\
            case 10: return 100\
            case 20: return 200\
            default: return 300\
        }\
    default: return 300\
        }",   
        300,
    },

    /* test #5 */
    {
        15,
        40,
        "\
switch (velocity) {\
    case 10: \
        switch (acceleration) {\
            case 10: return 100\
            case 20: return 300\
            default: return 400\
        }\
    case 20: \
        switch (acceleration) {\
            case 10: return 100\
            case 20: return 200\
            default: return 300\
        }\
    default: return 300\
        }",   
        350,
    },
};

int main() {
    NodeParser<Player> node_parser;
    node_parser.register_accessor("velocity", &Player::velocity);
    node_parser.register_accessor("acceleration", &Player::acceleration);

    for (int i = 0; i < sizeof(tests) / sizeof(TestCase); ++i) {
        Player player(tests[i].vel, tests[i].accel);
        Node<Player> *node = node_parser.parse(tests[i].str);

        if (!node) {
            fprintf(stderr, "Parsed test %d unsuccessfully\n", i);
            continue;
        }

        double result = construct_fuzzy_weigher(node)(&player);
        printf("Test %d: result=%f\n", i, result);
        if (!node || !approx_equal(result, tests[i].val)) {
            fprintf(stderr, "\nTest `%d` is broken\n", i); 
            break; 
        }
    }
    return 0;
}
