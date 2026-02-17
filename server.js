import { default as app, static as express_static } from "express";
const port = 7777;

app()
    .use(express_static("player", {
        //index: false,
        extensions: ["html"]
    }))
    .listen(port, console.log.bind(this, `Listening on: http://localhost:${ port }/`));
