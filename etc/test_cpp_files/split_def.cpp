
template <class T> struct data {
    T* ptr = new T[1000];

    [[gnu::noinline]] ~data() { delete[] ptr; }
};

struct cmState {

    data<int>  d;
    data<char> d2;

    ~cmState();
};

cmState::~cmState(){
    //puts("Hello, world!");

};

int main() {}
