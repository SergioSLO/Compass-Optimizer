#!/usr/bin/env python3
"""
Herramienta para comparar el plan generado por COMPASS-lite con el de PostgreSQL.

Requisitos:
- Haber compilado ./bin/compass_lite
- Tener un servidor PostgreSQL accesible (por defecto: postgresql://compass:compass@localhost:5432/compassdb)
- Tener psql en el PATH
"""

import argparse
import json
import re
import subprocess
from pathlib import Path


def run_command(cmd, input_text=None):
    result = subprocess.run(
        cmd,
        input=input_text,
        text=True,
        capture_output=True,
        check=True,
    )
    return result.stdout


def parse_number(text):
    text = text.strip()
    text = text.replace(" ", "").replace(",", "")
    try:
        return float(text)
    except ValueError:
        return None


def parse_compass_stats(output):
    stats = {"raw": output}
    patterns = {
        "cost": r"Costo acumulado:\s*([0-9eE\.\-]+)",
        "est_total": r"Cardinalidad estimada total\s*:\s*([0-9eE\.\-]+)",
        "real_total": r"Cardinalidad real total\s*:\s*([0-9eE\.\-]+)",
    }
    for key, pattern in patterns.items():
        match = re.search(pattern, output)
        if match:
            stats[key] = parse_number(match.group(1))
    return stats


def load_query(path):
    sql = Path(path).read_text(encoding="utf-8")
    sql = sql.strip().rstrip(";")
    return sql


def run_compass(args):
    cmd = [
        args.compass_bin,
        "--mode=both",
        "--data-dir",
        args.data_dir,
        args.query,
    ]
    output = run_command(cmd)
    return parse_compass_stats(output)


def run_postgres(args):
    sql = load_query(args.query)
    explain_sql = f"EXPLAIN (ANALYZE, FORMAT JSON) {sql};"
    cmd = [
        "psql",
        args.pg_url,
        "-X",
        "--tuples-only",
        "--no-align",
        "-A",
        "-c",
        explain_sql,
    ]
    stdout = run_command(cmd)
    json_line = None
    for line in stdout.splitlines():
        line = line.strip()
        if line.startswith("["):
            json_line = line
            break
    if not json_line:
        raise RuntimeError(f"No se encontró JSON en la salida de psql:\n{stdout}")
    data = json.loads(json_line)
    plan_root = data[0]["Plan"]
    stats = {
        "plan_rows": plan_root.get("Plan Rows"),
        "plan_width": plan_root.get("Plan Width"),
        "actual_rows": plan_root.get("Actual Rows"),
        "actual_time": plan_root.get("Actual Total Time"),
        "planning_time": data[0].get("Planning Time"),
        "execution_time": data[0].get("Execution Time"),
        "node_type": plan_root.get("Node Type"),
    }
    return stats


def main():
    parser = argparse.ArgumentParser(
        description="Compara el plan de COMPASS-lite contra PostgreSQL usando EXPLAIN ANALYZE."
    )
    parser.add_argument("query", help="Ruta a la consulta SQL.")
    parser.add_argument(
        "--data-dir",
        default="Data",
        help="Directorio donde se encuentran los CSV (por defecto: Data).",
    )
    parser.add_argument(
        "--compass-bin",
        default="./bin/compass_lite",
        help="Ruta al ejecutable de COMPASS-lite.",
    )
    parser.add_argument(
        "--pg-url",
        default="postgresql://compass:compass@localhost:5432/compassdb",
        help="Cadena de conexión a PostgreSQL para psql.",
    )
    args = parser.parse_args()

    print("== COMPASS-lite ==")
    compass_stats = run_compass(args)
    print(f"  Costo acumulado estimado : {compass_stats.get('cost')}")
    print(f"  Cardinalidad estimada     : {compass_stats.get('est_total')}")
    print(f"  Cardinalidad real (hash)  : {compass_stats.get('real_total')}")

    print("\n== PostgreSQL (EXPLAIN ANALYZE) ==")
    pg_stats = run_postgres(args)
    print(f"  Nodo raíz        : {pg_stats.get('node_type')}")
    print(f"  Filas estimadas  : {pg_stats.get('plan_rows')}")
    print(f"  Filas reales     : {pg_stats.get('actual_rows')}")
    print(f"  Tiempo ejecución : {pg_stats.get('execution_time')} ms")
    print(f"  Tiempo planificación : {pg_stats.get('planning_time')} ms")


if __name__ == "__main__":
    main()
