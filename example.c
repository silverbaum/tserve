#include "src/tserve.h"


void serve_index(int fd)
{
    struct route_file *index = load_file("index.html");
    get_respond(fd, index);
}



int main(void)
{
    app_init();
    GET_ROUTE("/", &serve_index);


    run(8000);
}
