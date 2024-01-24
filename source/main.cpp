# include <Siv3D.hpp> // Siv3D v0.6.13

struct PhysicsCircle {
    Vec2 pos;     // 重心位置
    Vec2 v;       // 速度
    double theta; // 回転角度
    double omega; // 角速度
    double M;     // 質量
    double I;     // 慣性モーメント
    double r;     // 半径

    PhysicsCircle() = default;

    PhysicsCircle(const Vec2& center, double radius) {
        pos   = center;
        r     = radius;
        v     = {};
        theta = 0;
        omega = 0;
        M     = 1;
        I     = 0.5 * M * r * r;
    }

    Circle circle() const {
        return Circle{ pos, r };
    }

    // 重心を基準とした相対座標から力積を加える
    void addImpulseLocal(const Vec2& impulse, const Vec2& addLocalPos) {
        v     += impulse / M;
        omega += addLocalPos.cross(impulse) / I;
    }

    // 絶対座標から力積を加える
    void addImpulse(const Vec2& impulse, const Vec2& addPos) {
        addImpulseLocal(impulse, addPos - pos);
    }

    void update(double delta) {
        pos   += v     * delta;
        theta += omega * delta;
    }

    void draw(const ColorF& color = Palette::White) {
        circle().draw();

        Line{ pos, Arg::direction = Circular(r, theta).toVec2() }.draw(Palette::Black);
    }
};

Color fruitColor(int32 n) {
    switch (n) {
    case 1:
        return Palette::Crimson;

    case 2:
        return Palette::Salmon;

    case 3:
        return Palette::Mediumorchid;

    case 4:
        return Color{ 255, 178, 0 };

    case 5:
        return Palette::Darkorange;

    case 6:
        return Palette::Red;

    case 7:
        return Palette::Khaki;

    case 8:
        return Palette::Pink;

    case 9:
        return Palette::Yellow;

    case 10:
        return Palette::Greenyellow;

    case 11:
        return Palette::Green;

    default:
        return Palette::White;
    }
}

double fruitR(int32 n) {
    return pow(1.2, n) * 12;
}

class Fruit : public PhysicsCircle {
    int32 m_num;

    bool m_dead   = false;
    bool m_fallen = false;

public:
    Fruit() = default;

    Fruit(const Vec2& pos, int32 n) : PhysicsCircle({ pos, fruitR(n) }) {
        m_num = n;
    }

    int32 num()const {
        return m_num;
    }

    bool dead() const {
        return m_dead;
    }

    void beDead() {
        m_dead = true;
    }

    bool fallen()const {
        return m_fallen;
    }

    void beFallen() {
        m_fallen = true;
    }

    void draw() const {
        Color c = fruitColor(m_num);

        circle().draw(c).drawFrame(2, HSV(c).setV(0.7));
    }
};

void Main() {
    Window::SetTitle(U"落ち物 マージ パズル | 移動: [A/D] 落とす: [Space]");

    Scene::SetBackground(Palette::Beige);

    Font font { FontMethod::MSDF, 30, Typeface::Bold };

    Array<std::unique_ptr<Fruit>> circles;

    Array<Line> walls;

    Rect box { 250, 120, 300, 380 };

    walls.emplace_back(box.bl(), box.br());
    walls.emplace_back(box.bl(), box.tl());
    walls.emplace_back(box.br(), box.tr());

    double accumulatorSec = 0;

    constexpr double stepSec = (1.0 / 200.0);
    constexpr int32 solveNum = 5;

    Vec2 grabPos = { 400, 80 };

    constexpr double grabSpeed = 150;
    constexpr Vec2 nextPos { 675, 200 };

    std::unique_ptr<Fruit> nextFruit = std::make_unique<Fruit>(nextPos, Random(1, 5));
    std::unique_ptr<Fruit> grabFruit;

    double grabWait = 0.5;

    std::unique_ptr<Fruit> addFruit;

    bool gameOver = false;

    int32 score = 0;

    while (System::Update()) {
        double delta = Scene::DeltaTime();

        if (not gameOver) {
            if (KeySpace.down() and grabFruit) {
                circles.emplace_back(std::move(grabFruit));

                grabWait = 0;
            }

            if (KeyLeft .pressed() || KeyA.pressed()) grabPos.x -= grabSpeed * delta;
            if (KeyRight.pressed() || KeyD.pressed()) grabPos.x += grabSpeed * delta;

            grabPos.x = Clamp(grabPos.x, 250.0, 550.0);

            if (grabFruit) {
                grabPos.x = Clamp(grabPos.x, 250.0 + grabFruit->r, 550.0 - grabFruit->r);

                grabFruit->pos = grabPos;
            }
            else {
                if (grabWait > 0.5) {
                    grabFruit = std::move(nextFruit);

                    grabFruit->pos = grabPos;

                    nextFruit = std::make_unique<Fruit>(nextPos, Random(1, 5));
                }

                grabWait += delta;
            }

            for (accumulatorSec += Scene::DeltaTime(); stepSec <= accumulatorSec; accumulatorSec -= stepSec) {
                for (auto& o : circles) {
                    o->update(stepSec);

                    o->v.y += 9.8; // 重力
                }

                for (auto& i : step(solveNum)) {
                    // Circle と Circle の衝突
                    for (auto& pc1 : circles) {
                        for (auto& pc2 : circles) {
                            Vec2 subV = pc1->pos - pc2->pos;

                            if (subV.isZero()) continue;

                            double overlap = (pc1->r + pc2->r - subV.length());

                            if (overlap < 0) continue;

                            Vec2 nv = subV.normalized();
                            Vec2 solveV = nv * overlap / 2;

                            pc1->pos += solveV;
                            pc2->pos -= solveV;

                            pc1->beFallen();
                            pc2->beFallen();

                            if (pc1->num() == pc2->num() and not addFruit) {
                                pc1->beDead();
                                pc2->beDead();

                                if (pc1->num() <= 10) addFruit = std::make_unique<Fruit>(pc1->pos - nv * pc1->r, pc1->num() + 1);

                                score += pc1->num() * (pc1->num() + 1) / 2;
                            }

                            double dotV = (pc2->v - pc1->v).dot(nv);

                            if (dotV < 0) continue;

                            Vec2 tan = nv.rotated90();
                            Vec2 fDir = -Sign((pc1->v - pc2->v).dot(tan) - pc1->r * pc1->omega - pc2->r * pc2->omega) * tan;
                            Vec2 impulse = (nv + fDir * 0.5) * Min(dotV, 50.0) * pc1->M * pc2->M / (pc1->M + pc2->M);

                            pc1->addImpulseLocal(impulse, -nv * pc1->r);
                            pc2->addImpulseLocal(-impulse, nv * pc2->r); // 反作用
                        }
                    }

                    // Circle と Wall の衝突
                    for (auto& pCircle : circles) {
                        for (auto& wall : walls) {
                            Vec2 subV = pCircle->pos - wall.closest(pCircle->pos);
                            if (subV.isZero()) continue;

                            double overlap = (pCircle->r - subV.length());

                            if (overlap < 0) continue; // overlapが正なら衝突

                            pCircle->beFallen(); // スイカゲームの処理

                            Vec2 nv = subV.normalized();

                            pCircle->pos += nv * overlap; // 重なり解消

                            double dotV = -pCircle->v.dot(nv); // 速度の衝突方向成分（衝突に向かう向きを正とする）

                            if (dotV < 0) continue; // 衝突方向に向かていないなら無視

                            Vec2 tan = nv.rotated90();
                            Vec2 fDir = -Sign(pCircle->v.dot(tan) - pCircle->r * pCircle->omega) * tan; // 摩擦の方向ベクトル

                            pCircle->addImpulseLocal((nv + fDir * 0.5) * Min(dotV, 50.0) * pCircle->M, -nv * pCircle->r); // 摩擦＋垂直効力
                        }
                    }

                    circles.remove_if([](const auto& o) { return o->dead(); });
                    if (addFruit) circles.emplace_back(std::move(addFruit));
                }
            }

            for (auto& o : circles) {
                if (o->fallen() and o->pos.y - o->r < 80 or o->pos.y > 800) gameOver = true;
            }
        }
        else {
            // リトライ
            if (KeySpace.down()) {
                circles.clear();

                grabFruit.reset();

                nextFruit = std::make_unique<Fruit>(nextPos, Random(1, 5));

                grabWait = 0.5;

                grabPos = { 400, 80 };

                gameOver = false;

                score = 0;
            }
        }

        ////////////////////////////////////////////////////////////////////////////////////////////////////

        Rect{ 800, 400 }.draw(Palette::Burlywood);

        box.draw(ColorF(1, 0.5));

        for (auto& o : circles) {
            o->draw();
        }

        for (auto& o : walls) {
            o.draw(3, Palette::Brown);
        }

        Circle{ grabPos, 10 }.draw();

        if (grabFruit) grabFruit->draw();
        if (nextFruit) nextFruit->draw();

        font(U"スコア: ", score).draw(30, 40, 100);

        font(U"ネクスト").drawAt(30, nextPos.x, nextPos.y - 80);

        if (gameOver) {
            Scene::Rect().draw(ColorF(0, 0.75));

            font(U"スコア: ", score).drawAt(60, 400, 300);
            font(U"Spaceキーでリトライ").drawAt(20, 400, 340);
        }

        font(U"© 2023 kanaaa224.").drawAt(12, 400, 585, Palette::Black);
    }
}
