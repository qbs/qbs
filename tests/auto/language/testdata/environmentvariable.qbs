import qbs.Environment

Product {
    name: Environment.getEnv("PRODUCT_NAME")
}
