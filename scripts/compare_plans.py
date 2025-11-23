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
import time
from pathlib import Path


def run_command(cmd, input_text=None):
    result = subprocess.run(
        cmd,
        input=input_text,
        text=True,
        capture_output=True,
    )
    if result.returncode != 0:
        raise RuntimeError(
            f"Error al ejecutar {' '.join(cmd)}\nSTDOUT:\n{result.stdout}\nSTDERR:\n{result.stderr}"
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


def build_compass_command(args):
    cmd = [
        args.compass_bin,
        "--mode=both",
        "--data-dir",
        args.data_dir,
    ]
    if args.compass_plan_dot:
        cmd.append(f"--plan-dot={args.compass_plan_dot}")
    if args.compass_plan_png:
        cmd.append(f"--plan-png={args.compass_plan_png}")
    cmd.append(args.query)
    return cmd


def run_compass(args):
    cmd = build_compass_command(args)
    start = time.perf_counter()
    output = run_command(cmd)
    elapsed_ms = (time.perf_counter() - start) * 1000.0
    stats = parse_compass_stats(output)
    stats["full_output"] = output
    stats["plan_time_ms"] = elapsed_ms
    return stats


def run_postgres(args):
    sql = load_query(args.query)
    explain_sql = f"EXPLAIN (FORMAT JSON) {sql};"
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
    start = time.perf_counter()
    stdout = run_command(cmd)
    elapsed_ms = (time.perf_counter() - start) * 1000.0
    json_blob = None
    for idx, line in enumerate(stdout.splitlines()):
        stripped = line.lstrip()
        if stripped.startswith("["):
            json_blob = "\n".join(stdout.splitlines()[idx:]).strip()
            break
    if not json_blob:
        raise RuntimeError(f"No se encontró JSON en la salida de psql:\n{stdout}")
    data = json.loads(json_blob)
    plan_root = data[0]["Plan"]
    stats = {
        "plan_rows": plan_root.get("Plan Rows"),
        "plan_width": plan_root.get("Plan Width"),
        "startup_cost": plan_root.get("Startup Cost"),
        "total_cost": plan_root.get("Total Cost"),
        "actual_rows": plan_root.get("Actual Rows"),
        "planning_time": data[0].get("Planning Time"),
        "node_type": plan_root.get("Node Type"),
        "raw": stdout,
        "elapsed_ms": elapsed_ms,
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
        "--compass-plan-dot",
        default="",
        help="Ruta para exportar el plan en formato DOT (opcional).",
    )
    parser.add_argument(
        "--compass-plan-png",
        default="",
        help="Ruta para exportar el plan en PNG (opcional).",
    )
    parser.add_argument(
        "--pg-analyze",
        action="store_true",
        help="Si se especifica, ejecuta EXPLAIN ANALYZE en lugar de solo EXPLAIN.",
    )
    parser.add_argument(
        "--pg-url",
        default="postgresql://compass:compass@localhost:5432/compassdb",
        help="Cadena de conexión a PostgreSQL para psql.",
    )
    args = parser.parse_args()

    print("== COMPASS-lite ==")
    compass_stats = run_compass(args)
    print("--- Salida completa ---")
    print(compass_stats.get("full_output", ""))
    print("\n--- Resumen ---")
    print(f"  Costo acumulado estimado : {compass_stats.get('cost')}")
    print(f"  Cardinalidad estimada     : {compass_stats.get('est_total')}")
    plan_time = compass_stats.get("plan_time_ms")
    if plan_time is not None:
        print(f"  Tiempo de planificación   : {plan_time:.2f} ms")

    print("\n== PostgreSQL (EXPLAIN) ==")
    pg_stats = run_postgres(args)
    print("--- Resumen ---")
    print(f"  Nodo raíz             : {pg_stats.get('node_type')}")
    print(f"  Filas (est/reales)    : {pg_stats.get('plan_rows')} / {pg_stats.get('actual_rows')}")
    print(f"  Costos (inicio/total) : {pg_stats.get('startup_cost')} / {pg_stats.get('total_cost')}")
    print(f"  Tiempo planificación  : {pg_stats.get('planning_time')} ms")
    elapsed = pg_stats.get("elapsed_ms")
    if elapsed is not None:
        print(f"  Tiempo total comando  : {elapsed:.2f} ms")


if __name__ == "__main__":
    main()
