
// This is a convenience product to be able to use Qt Creator for editing the docker files.
// For building and managing the images, use docker-compose as explained in
// https://doc.qt.io/qbs/building-qbs.html#using-docker.
Product {
    name: "docker"
    files: "**"
}
