Module {
    // This indirection exists to properly model QBS-1776.
    // "deploader" corresponds to "bundle", and "dep" corresponds to "codesign"
    Depends { condition: project.useExistingModule; name: "dep"; required: false }

    Depends { condition: !project.useExistingModule; name: "random"; required: false }
}
