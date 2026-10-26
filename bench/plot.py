#!/usr/bin/env python3
"""Plot the recorded CPU measurements; requires matplotlib only for export."""
import json
from pathlib import Path

import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt

DIRECTORY = Path(__file__).resolve().parent


def aggregate(pattern):
    groups = {}
    for path in DIRECTORY.glob(pattern):
        for row in json.loads(path.read_text()):
            if row['fps'] is not None:
                groups.setdefault((row['width'], row['height'], row['size']), []).append(row)
    results = []
    for rows in groups.values():
        result = dict(rows[0])
        seconds = sum(row['elapsed_s'] for row in rows)
        for key in ('client_cpu', 'graphics_cpu', 'total_cpu', 'fps'):
            result[key] = sum(row[key] * row['elapsed_s'] for row in rows) / seconds
        results.append(result)
    return results


def main():
    desktop = sorted(aggregate('results-desktop*.json'), key=lambda row: row['cells'])
    virtual = sorted((row for row in aggregate('results-xvfb*.json') if row['size'] == 0),
                     key=lambda row: row['width'])
    fig, axes = plt.subplots(1, 2, figsize=(13, 5.8), gridspec_kw={'width_ratios': [1.5, 1]})
    for ax, rows, title in zip(axes, (desktop, virtual),
                              ('Écran réel · 1920 × 1080', 'Xvfb logiciel · taille automatique')):
        labels = [f"{row['columns']} × {row['rows']} cellules\n{row['actual_size']} px/cellule"
                  if rows is desktop else f"{row['width']} × {row['height']} pixels\n{row['cells']:,} cellules".replace(',', ' ')
                  for row in rows]
        y = list(range(len(rows)))
        client = [row['client_cpu'] for row in rows]
        graphics = [row['graphics_cpu'] for row in rows]
        ax.barh(y, client, color='#35a99a', label='Programme', height=.65)
        ax.barh(y, graphics, left=client, color='#c99751', label='Serveur graphique', height=.65)
        for index, row in enumerate(rows):
            ax.text(row['total_cpu'] + .65, index,
                    f"{row['total_cpu']:.1f} % · {row['fps']:.1f} i/s", va='center', fontsize=9)
        ax.axvline(10, color='#555555', linestyle='--', linewidth=1)
        ax.set_yticks(y, labels)
        ax.invert_yaxis()
        ax.set_xlim(0, max(row['total_cpu'] for row in rows) * 1.38)
        ax.set_title(title, loc='left', fontweight='bold', pad=15)
        ax.set_xlabel('CPU total (% d’un cœur logique)')
        ax.set_axisbelow(True)
        ax.grid(axis='x', alpha=.2)
        ax.spines[['top', 'right', 'left']].set_visible(False)
        ax.tick_params(axis='y', length=0)
    axes[0].legend(loc='upper right', frameon=False)
    fig.suptitle('Game of Life — coût de la finesse et de la résolution', fontsize=16, fontweight='bold')
    fig.text(.5, .02, 'Core i7-8750H · cible 15 i/s · trait pointillé : budget de 10 % · mesures avec barrière XSync',
             ha='center', fontsize=9, color='#555555')
    fig.tight_layout(rect=(0, .055, 1, .93), w_pad=3)
    fig.savefig(DIRECTORY / 'results.png', dpi=180)
    fig.savefig(DIRECTORY / 'results.svg')


if __name__ == '__main__':
    main()
