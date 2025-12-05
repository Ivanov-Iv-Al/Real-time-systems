# generate_plots.py
import matplotlib.pyplot as plt
import numpy as np
import subprocess
import sys

def run_task1():
    """Запуск задания 1 и сбор данных"""
    print("Запуск Task 1...")
    try:
        result = subprocess.run(['./task1_latency'], 
                               capture_output=True, text=True)
        data = []
        for line in result.stdout.split('\n'):
            if line and line[0].isdigit():
                parts = line.split()
                if len(parts) >= 4:
                    iteration = int(parts[0])
                    latency = int(parts[1])
                    minor_faults = int(parts[2])
                    major_faults = int(parts[3])
                    data.append([iteration, latency, minor_faults, major_faults])
        
        return np.array(data)
    except Exception as e:
        print(f"Ошибка: {e}")
        return None

def generate_plots(data_task1):
    """Генерация графиков"""
    if data_task1 is None or len(data_task1) == 0:
        print("Нет данных для построения графиков")
        return
    
    # График 1: Латентность по итерациям
    plt.figure(figsize=(12, 10))
    
    # Латентность
    plt.subplot(3, 1, 1)
    plt.plot(data_task1[:, 0], data_task1[:, 1], 'b-', alpha=0.7, linewidth=0.5)
    plt.xlabel('Итерация')
    plt.ylabel('Латентность (нс)')
    plt.title('Task 1: Латентность доступа к памяти с page faults')
    plt.grid(True, alpha=0.3)
    
    # Minor faults
    plt.subplot(3, 1, 2)
    plt.plot(data_task1[:, 0], data_task1[:, 2], 'g-', alpha=0.7, linewidth=0.5)
    plt.xlabel('Итерация')
    plt.ylabel('Minor faults')
    plt.title('Minor Page Faults')
    plt.grid(True, alpha=0.3)
    
    # Major faults (обычно 0 в этом эксперименте)
    plt.subplot(3, 1, 3)
    plt.plot(data_task1[:, 0], data_task1[:, 3], 'r-', alpha=0.7, linewidth=0.5)
    plt.xlabel('Итерация')
    plt.ylabel('Major faults')
    plt.title('Major Page Faults')
    plt.grid(True, alpha=0.3)
    
    plt.tight_layout()
    plt.savefig('task1_results.png', dpi=300, bbox_inches='tight')
    print("Графики сохранены в task1_results.png")
    
    # Гистограмма латентности
    plt.figure(figsize=(10, 6))
    plt.hist(data_task1[:, 1], bins=50, alpha=0.7, color='blue', edgecolor='black')
    plt.xlabel('Латентность (нс)')
    plt.ylabel('Частота')
    plt.title('Task 1: Распределение латентности')
    plt.grid(True, alpha=0.3)
    plt.savefig('task1_histogram.png', dpi=300, bbox_inches='tight')
    
    # Статистика
    print("\n=== Статистика Task 1 ===")
    print(f"Средняя латентность: {np.mean(data_task1[:, 1]):.2f} нс")
    print(f"Максимальная латентность: {np.max(data_task1[:, 1]):.2f} нс")
    print(f"Минимальная латентность: {np.min(data_task1[:, 1]):.2f} нс")
    print(f"Стандартное отклонение: {np.std(data_task1[:, 1]):.2f} нс")
    print(f"Общее minor faults: {np.sum(data_task1[:, 2])}")
    print(f"Общее major faults: {np.sum(data_task1[:, 3])}")

if __name__ == "__main__":
    # Сначала нужно собрать программы
    print("Сборка программ...")
    subprocess.run(['make', 'clean'])
    subprocess.run(['make'])
    
    # Запуск Task 1 и сбор данных
    data = run_task1()
    
    if data is not None:
        # Сохранение сырых данных
        np.savetxt('task1_data.csv', data, 
                  delimiter=',', 
                  header='iteration,latency_ns,minor_faults,major_faults')
        print("Данные сохранены в task1_data.csv")
        
        # Генерация графиков
        generate_plots(data)
        
        print("\nДля запуска Task 2 (требуется sudo):")
        print("  make run_task2")
        print("\nДля запуска Task 3 (требуется sudo):")
        print("  make run_task3")
    else:
        print("Не удалось получить данные от Task 1")
        sys.exit(1)