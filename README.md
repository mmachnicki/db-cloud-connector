# db-cloud-connector
Cloud connector for private databases, requires active socket on DB side.

All clients connecting to the connector gets an ID. One of them is a db client which are all requests passed to. 
When a response is provided from the db client, connector checks the response's id and redirects it to the appropriate client, 
selected by response ID.

An example php client provided.
