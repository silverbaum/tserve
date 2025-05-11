#include "src/tserve.h"
#include <string.h>


static char* data = "";

void serve_index(struct request *req)
{
    struct route_file *index = load_file("index.html");
    get_respond(req, index);
}

void serve_data(struct request *req)
{
    get_respond_text(req, data);
}

void post_route(struct request *req){
    data = strdup(req->body);
    get_respond_json(req, "{\"message\": \"success\"}");
}


int main(void)
{
    app_init();
    GET_ROUTE("/", &serve_index);
    GET_ROUTE("/data", &serve_data);
    POST_ROUTE("/request", &post_route);


    run(8000);
}
