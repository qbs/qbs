QtApplication {
    files: "main.cpp"
    Group {
        name: "QObject service provider"
        files: "createqtclass.h"
        fileTags: ["hpp", "unmocable"]
    }
    Group {
        name: "QObject service user"
        files: "myqtclass.h"
        fileTags: ["hpp", "mocable"]
    }
}
