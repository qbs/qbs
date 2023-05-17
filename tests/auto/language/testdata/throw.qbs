Project {
    property string throwType
    property bool dummy: {
        if (throwType === "bool")
            throw true;
        if (throwType === "int")
            throw 43;
        if (throwType === "string")
            throw "an error";
        if (throwType === "list")
            throw ["an", "error"];
        if (throwType === "object")
            throw { result: "crash", reason: "overheating" };
        throw "type missing";
    }
}
