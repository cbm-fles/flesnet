# Log collection for flesnet

A test setup that collects the JSON log files of the flesnet programs (see
[doc/logging.md](../../doc/logging.md)) in OpenSearch:

```
flesnet programs --> local log files --> Fluent Bit --> OpenSearch --> Dashboards
   (each node)        (each node)        (each node)   (login node)    Grafana
```

- OpenSearch and OpenSearch Dashboards run on the login node in Docker,
  see [compose.yaml](compose.yaml).
- Fluent Bit runs natively on each node and sends the log files to OpenSearch,
  see [fluent-bit/](fluent-bit/).
- Grafana on the login node reads OpenSearch, see [grafana/](grafana/).

It is adapted from the DCA logging setup in
`walters-test-environment/loggingstack-test`, without Kafka: the log files and
the file buffer of Fluent Bit take its role. The field names of flesnet follow
the DCA logger.

## Indices

| Index | Messages | Kept for |
|---|---|---|
| `logs-flesnet-oper-YYYY.MM` | status and above | `OPENSEARCH_OPER_RETENTION` (90 days) |
| `logs-flesnet-debug-YYYY.MM.DD` | debug and trace | `OPENSEARCH_DEBUG_RETENTION` (7 days) |

`logs-flesnet-*` covers both. The index template stores strings as keywords,
`message` as text, `@timestamp` with full precision and `data` as a
`flat_object`: its keys do not become fields, and its values are strings that
can be searched, e.g. `data.build_time_ms:1001`. `data_text` is a readable copy
of `data`. Fluent Bit adds `log_file`, the path of the file a message comes from.

## OpenSearch on the login node

```bash
cp .env.example .env        # set the passwords, check the limits
chmod 600 .env
docker compose up -d
./setup-opensearch          # once, before logs arrive, and after changing it
```

OpenSearch listens on localhost and on the cluster network address
(`OPENSEARCH_CLUSTER_ADDRESS`) for Fluent Bit; Dashboards only on localhost:

```bash
ssh -L 5601:localhost:5601 login     # then http://localhost:5601, user admin
```

The data is in the Docker volumes `loggingstack_opensearch-data` and
`loggingstack_dashboards-data`. Note that OpenSearch stops writing when its
disk is more than 95 % full. `docker compose down -v` removes everything.

## Fluent Bit on the nodes

Install Fluent Bit (from packages.fluentbit.io, version 5.1) into the node
image, then:

```bash
install -d /etc/fluent-bit
install -m 644 fluent-bit/fluent-bit.conf fluent-bit/parsers-flesnet.conf /etc/fluent-bit/
install -m 600 fluent-bit/opensearch.env.example /etc/fluent-bit/opensearch.env
# set OPENSEARCH_PASSWORD to OPENSEARCH_FLUENTBIT_PASSWORD from .env, and
# FLESNET_LOG_DIR to where the programs write their logs
mkdir -p /etc/systemd/system/fluent-bit.service.d
cat > /etc/systemd/system/fluent-bit.service.d/flesnet.conf <<'EOF'
[Service]
EnvironmentFile=/etc/fluent-bit/opensearch.env
StateDirectory=fluent-bit
ExecStartPre=/bin/mkdir -p ${FLESNET_LOG_DIR}
EOF
systemctl enable fluent-bit
```

The programs write their log files to `FLESNET_LOG_DIR` or a subdirectory of
it, e.g. one per run, with `--log-file DIR/NAME.log` (or `CBM_LOG_FILE`). The
name should tell the program and its instance apart, e.g.
`node10_tsclient_0.log`; `host`, `service` and `pid` are in each message anyway.
Fluent Bit keeps its read positions and buffered messages in
`FLUENT_BIT_STATE_DIR`. On nodes without a disk, both directories are in memory
and lost at a reboot, and their size limits the amount of logs.

To try the configuration without installing it, run it in Docker:

```bash
docker run --rm --network host -v $PWD/fluent-bit:/fluent-bit/etc:ro \
  -v LOGDIR:/logs:ro -v STATEDIR:/state -e FLESNET_LOG_DIR=/logs \
  -e FLUENT_BIT_STATE_DIR=/state -e OPENSEARCH_HOST=10.0.101.1 \
  -e OPENSEARCH_PASSWORD=... docker.io/fluent/fluent-bit:5.1.5 \
  -c /fluent-bit/etc/fluent-bit.conf
```

## Grafana

Grafana reads OpenSearch as the user `grafana`, which may only read the logs.

```bash
sudo grafana-cli plugins install grafana-opensearch-datasource
( set -a; . ./.env; envsubst '$OPENSEARCH_GRAFANA_PASSWORD' \
    < grafana/opensearch-flesnet.yaml ) \
  | sudo install -m 640 -g grafana /dev/stdin \
    /etc/grafana/provisioning/datasources/opensearch-flesnet.yaml
sudo systemctl restart grafana-server
```

In Explore, choose the data source and the query type `Logs`. The dashboard
[grafana/flesnet-logs.json](grafana/flesnet-logs.json) shows the messages by
severity, host and kind, and the messages themselves; import it with
Dashboards → New → Import.
