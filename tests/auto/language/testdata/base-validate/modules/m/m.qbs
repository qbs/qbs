MParent {
    condition: true
    validate: {
        var parentResult = base;
        if (!parentResult)
            throw "Parent failed";
        throw "Parent succeeded, child failed.";
    }
}
