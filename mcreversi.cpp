#include <cassert>
#include <ctime>
#include <algorithm>
#include <iostream>
#include <sstream>
#include <vector>
#include <array>
#include <iterator>
#include <memory>
#include <random>

const size_t BOARD_SIZE  = 8;
const size_t BOARD_CELLS = BOARD_SIZE * BOARD_SIZE;
const std::string INITIAL_BOARD =
"........"
"........"
"........"
"...OX..."
"...XO..."
"........"
"........"
"........";
const size_t MIN_VISITS_TO_EXPAND = 1;
const float EXPLORATION_CONST = M_SQRT2;

enum class Player {
    BLACK, WHITE
};

enum class Cell {
    BLACK, WHITE, EMPTY
};

char typeToChar(Cell type) {
    switch (type) {
    case Cell::BLACK:
        return 'X';
    case Cell::WHITE:
        return 'O';
    case Cell::EMPTY:
        return '.';
    }
    assert(false && "unreachable");
}

Cell parseCell(char chr) {
    switch (tolower(chr)) {
    case 'x':
        return Cell::BLACK;
    case 'o':
        return Cell::WHITE;
    case '.':
        return Cell::EMPTY;
    }
    assert(false && "unreachable");
}

class Board {
public:
    Board(const std::string& str) {
        size_t idx = 0;
        for (const char chr : str) {
            cells_.at(idx++) = parseCell(chr);
        }
        assert(idx == BOARD_CELLS);
    }

    Board() : Board(INITIAL_BOARD) {}

    Board(const Board& board) {
        cells_ = board.cells_;
    }

    virtual ~Board() = default;

    Cell& at(size_t x, size_t y) {
        return cells_.at(x + y * BOARD_SIZE);
    }

    Cell at(size_t x, size_t y) const {
        return cells_.at(x + y * BOARD_SIZE);
    }

    bool isFilled() const {
        return std::all_of(cells_.begin(), cells_.end(), [](const Cell cell) {
            return cell != Cell::EMPTY;
        });
    }

    float getBlackOccupation() const {
        size_t black = 0;
        for (const auto& cell : cells_) {
            if (cell == Cell::BLACK) {
                ++black;
            }
        }
        return static_cast<float>(black) / BOARD_CELLS;
    }

    void flipPlayer() {
        for (auto& cell : cells_) {
            switch (cell) {
            case Cell::BLACK:
                cell = Cell::WHITE;
                break;
            case Cell::WHITE:
                cell = Cell::BLACK;
                break;
            default:
                break;
            }
        }
    }

    std::vector<Board> getNextStates() const {
        std::vector<Board> boards;
        boards.reserve(BOARD_CELLS);
        Board tmp = *this;
        for (size_t y = 0; y < BOARD_SIZE; ++y) {
            for (size_t x = 0; x < BOARD_SIZE; ++x) {
                if (tmp.put(x, y)) {
                    boards.push_back(tmp);
                    boards.back().flipPlayer();
                }
                tmp = *this;
            }
        }
        boards.shrink_to_fit();
        return boards;
    }

    Board flipped() const {
        Board board = *this;
        board.flipPlayer();
        return board;
    }

    bool put(size_t x, size_t y) {
        if (!isInBound(x, y) || at(x, y) != Cell::EMPTY) {
            return false;
        }

        at(x, y) = Cell::BLACK;

        bool valid = false;
        for (int dx = -1; dx <= 1; ++dx) {
            for (int dy = -1; dy <= 1; ++dy) {
                if (dx == 0 && dy == 0) {
                    continue;
                }

                size_t n = 1;
                bool blackFound = false;
                for (; isInBound(x, y, n * dx, n * dy); ++n) {
                    Cell state = at(x + n * dx, y + n * dy);
                    if (state == Cell::BLACK) {
                        blackFound = true;
                    }
                    if (state != Cell::WHITE) {
                        break;
                    }
                }
                if (blackFound && n > 1) {
                    valid = true;
                    for (size_t i = 1; i < n; ++i) {
                        at(x + i * dx, y + i * dy) = Cell::BLACK;
                    }
                }
            }
        }

        return valid;
    }

    void print() const {
        std::cout << "  ";
        for (size_t x = 0; x < BOARD_SIZE; ++x) {
            std::cout << static_cast<char>(x + 'a');
        }
        std::cout << std::endl << " +";
        for (size_t x = 0; x < BOARD_SIZE; ++x) {
            std::cout << '-';
        }
        std::cout << std::endl;
        for (size_t y = 0; y < BOARD_SIZE; ++y) {
            std::cout << y + 1 << '|';
            for (size_t x = 0; x < BOARD_SIZE; ++x) {
                std::cout << typeToChar(at(x, y));
            }
            std::cout << std::endl;
        }
    }

private:
    std::array<Cell, BOARD_CELLS> cells_;

    static bool isInBound(size_t x, size_t y, int dx = 0, int dy = 0) {
        if (dx < 0 && static_cast<size_t>(-dx) > x) {
            return false;
        }
        if (dy < 0 && static_cast<size_t>(-dy) > y) {
            return false;
        }
        return (x + dx < BOARD_SIZE) && (y + dy < BOARD_SIZE);
    }
};

class RNG {
public:
    RNG() {
        std::random_device rnd;
        engine = std::mt19937(rnd());
    }

    virtual ~RNG() = default;

    static RNG& getSingleton() {
        static RNG instance;
        return instance;
    }

    size_t randomIndex(size_t n) {
        assert(n > 0);
        if (n == 1) {
            return 0;
        } else {
            std::uniform_int_distribution<size_t> dist(0, n - 1);
            return dist(engine);
        }
    }

private:
    std::mt19937 engine;
};

class Node {
public:
    Node(const Board& board, Player player, Node* parent) :
        board_(board),
        player_(player),
        isPassMove_(false),
        games_(0),
        mean_(0),
        parent_(parent) {}

    virtual ~Node() = default;

    bool isLeafNode() const {
        return children_.empty()
            || board_.isFilled()
            || (parent_ && isPassMove_ && parent_->isPassMove_);
    }

    void playout() {
        Board current = board_;
        Player player = player_;
        bool passed = isPassMove_;
        while (!current.isFilled()) {
            const auto& boards = current.getNextStates();
            if (boards.empty()) {
                if (passed) {
                    break;
                }
                passed = true;
                current = current.flipped();
            } else {
                passed = false;
                const size_t idx = RNG::getSingleton().randomIndex(boards.size());
                current = boards.at(idx);
            }
            player = (player == Player::BLACK) ? Player::WHITE : Player::BLACK;
        }

        const float occ = current.getBlackOccupation();
        if (player == player_) {
            propagateResult(1 - occ);
        } else {
            propagateResult(occ);
        }
    }

    void expand() {
        if (isPassMove_) {
            assert(parent_);
            if (parent_->isPassMove_) {
                return;
            }
        }
        if (children_.empty() && games_ >= MIN_VISITS_TO_EXPAND) {
            const auto nextPlayer = (player_ == Player::BLACK) ? Player::WHITE : Player::BLACK;
            const auto& boards = board_.getNextStates();
            isPassMove_ = boards.empty();
            if (isPassMove_) {
                assert(parent_);
                if (!parent_->isPassMove_) {
                    children_.emplace_back(std::make_shared<Node>(board_.flipped(), nextPlayer, this));
                }
            } else {
                for (const auto& board : boards) {
                    children_.emplace_back(std::make_shared<Node>(board, nextPlayer, this));
                }
            }
        }
    }

    std::weak_ptr<Node> getChildWithMaxUCB() const {
        return getChildWithMaxValue<float>([](const std::shared_ptr<Node> child) {
            return child->calcUCB();
        });
    }

    std::weak_ptr<Node> getChildWithMaxVisits() const {
        return getChildWithMaxValue<size_t>([](const std::shared_ptr<Node> child) {
            return child->games_;
        });
    }

    const Board& getBoard() const {
        return board_;
    }

    size_t getNumGames() const {
        return games_;
    }

    float getExpectedOccupation() const {
        return 1 - mean_;
    }

private:
    Board board_;
    Player player_;
    bool isPassMove_;
    size_t games_;
    float mean_;

    Node* parent_;
    std::vector<std::shared_ptr<Node>> children_;

    float calcUCB() const {
        assert(parent_);
        assert(games_ <= parent_->games_);

        if (games_ == 0) {
            return INFINITY;
        } else {
            float bias = EXPLORATION_CONST * sqrtf(logf(parent_->games_) / games_);
            return mean_ + bias;
        }
    }

    void propagateResult(float occ) {
        mean_ = (games_ * mean_ + occ) / (games_ + 1);
        ++games_;
        if (parent_) {
            parent_->propagateResult(1 - occ);
        }
    }

    template<typename T>
    std::weak_ptr<Node> getChildWithMaxValue(std::function<T(std::shared_ptr<Node>)> eval) const {
        assert(!children_.empty());

        std::vector<T> values;
        values.reserve(children_.size());
        std::transform(children_.begin(), children_.end(), std::back_inserter(values), [&eval] (std::weak_ptr<Node> child) {
            const std::shared_ptr<Node> c = child.lock();
            assert(c);
            return eval(c);
        });
        const size_t i = std::distance(values.begin(), std::max_element(values.begin(), values.end()));

        return children_.at(i);
    }
};

Board searchMove(const Board& board, float time_sec) {
    const auto& boards = board.getNextStates();
    switch (boards.size()) {
    case 0:
        return board.flipped();
    case 1:
        return boards.front();
    default:
        break;
    }

    const auto root = std::make_shared<Node>(board, Player::BLACK, nullptr);
    root->expand();

    const auto start = clock();
    while (clock() - start < time_sec * CLOCKS_PER_SEC) {
        std::weak_ptr<Node> current = root;
        while (true) {
            auto c = current.lock();
            assert(c);

            if (c->isLeafNode()) {
                c->playout();
                c->expand();
                break;
            } else {
                current = c->getChildWithMaxUCB();
            }
        }
    }

    std::cout << "#games: " << root->getNumGames() << ", occupation: " << root->getExpectedOccupation() << std::endl;

    const auto c = root->getChildWithMaxVisits().lock();
    assert(c);
    return c->getBoard();
}

int main(int argc, char** argv) {
    float time = 1;
    if (argc >= 2) {
        time = atof(argv[1]);
    }

    Board current;
    current.print();

    char str[2];
    while (!current.isFilled()) {
        while (true) {
            std::cout << "move? ";
            std::cin >> str;
            const size_t x = tolower(str[0]) - 'a';
            const size_t y = str[1] - '1';
            if (current.put(x, y)) {
                break;
            }
        }
        current.print();
        current.flipPlayer();

        current = searchMove(current, time);
        current.print();
    }

    return 0;
}
