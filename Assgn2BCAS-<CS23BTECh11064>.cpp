#include <iostream>
#include <vector>
#include <fstream>
#include <pthread.h>
#include <atomic>
#include <chrono>
#include <cmath>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <random>
#include <thread>

using namespace std;
using namespace chrono;

int K, N, taskInc;
vector<vector<int>> sudoku;
atomic<int> C(0);
atomic<bool> sudoku_valid(true);
atomic<bool> lock_var(false);
ofstream output_file("output.txt");

vector<double> entrytimes, exittimes;
double worstentrytime = 0, worstexittime = 0;

double average(const vector<double> &times)
{
    if (times.empty())
        return 0.0;

    double sum = 0;
    for (size_t i = 0; i < times.size(); i++)
    {
        sum += times[i];
    }

    return sum / times.size();
}

string current_time()
{
    auto now = system_clock::now();
    time_t now_c = system_clock::to_time_t(now);
    tm *ltm = localtime(&now_c);
    stringstream ss;
    ss << put_time(ltm, "%H:%M:%S");
    return ss.str();
}

struct thread_data
{
    int id;
    thread_data(int id) : id(id) {}
};

void delay_app(int delay)
{
    this_thread::sleep_for(microseconds(delay));
    delay = min(delay * 2, 1000);
}

void Bounded_CAS_lock(int id)
{
    auto request_time = high_resolution_clock::now();
    bool expected = false;
    int delay = 1;

    output_file << "Thread " << id << " requests to enter CS at " << current_time() << "\n";

    while (!lock_var.compare_exchange_strong(expected, true, memory_order_acquire))
    {
        expected = false;
        this_thread::yield();
    }

    auto enter_time = high_resolution_clock::now();
    double entry_duration = duration_cast<microseconds>(enter_time - request_time).count();

    entrytimes.push_back(entry_duration);
    worstentrytime = max(worstentrytime, entry_duration);

    output_file << "Thread " << id << " enters CS at " << current_time() << "\n";
}

void Bounded_CAS_unlock(int id)
{
    lock_var.store(false, memory_order_release);
    output_file << "Thread " << id << " exits CS at " << current_time() << "\n";
}

void *check_sudoku(void *arg)
{
    thread_data *data = (thread_data *)arg;
    int id = data->id;

    while (sudoku_valid.load())
    {
        Bounded_CAS_lock(id);
        int task = C.fetch_add(taskInc);
        Bounded_CAS_unlock(id);

        if (task >= N)
            break;

        vector<bool> visited(N, false);
        bool valid = true;

        if (id < K)
        {
            Bounded_CAS_lock(id);
            output_file << "Thread " << id << " grabs row " << task + 1 << " at " << current_time() << "\n";
            Bounded_CAS_unlock(id);
            for (int j = 0; j < N; j++)
            {
                int index = sudoku[task][j] - 1;
                if (index < 0 || index >= N || visited[index])
                {
                    valid = false;
                    break;
                }
                visited[index] = true;
            }
            Bounded_CAS_lock(id);
            output_file << "Thread " << id << " completes checking of row " << (task + 1)
                        << " at " << current_time() << " hrs and finds it as ";
            if (valid)
            {
                output_file << "valid.\n";
            }
            else
            {
                output_file << "invalid.\n";
            }

            Bounded_CAS_unlock(id);
        }
        else
        {
            Bounded_CAS_lock(id);
            output_file << "Thread " << id << " grabs column " << task + 1 << " at " << current_time() << "\n";
            Bounded_CAS_unlock(id);
            for (int j = 0; j < N; j++)
            {
                int index = sudoku[j][task] - 1;
                if (index < 0 || index >= N || visited[index])
                {
                    valid = false;
                    break;
                }
                visited[index] = true;
            }
            Bounded_CAS_lock(id);
            output_file << "Thread " << id << " completes checking of row " << (task + 1)
                        << " at " << current_time() << " hrs and finds it as ";
            if (valid)
            {
                output_file << "valid.\n";
            }
            else
            {
                output_file << "invalid.\n";
            }

            Bounded_CAS_unlock(id);
        }

        if (!valid)
        {
            sudoku_valid.store(false, memory_order_relaxed);
            break;
        }
    }

    return NULL;
}

int main()
{

    ifstream input_file("input.txt");
    input_file >> K >> N >> taskInc;
    sudoku.resize(N, vector<int>(N));

    for (int i = 0; i < N; i++)
    {
        for (int j = 0; j < N; j++)
        {
            input_file >> sudoku[i][j];
        }
    }

    vector<pthread_t> threads(3 * K);
    vector<thread_data *> thread_data_ptrs(3 * K);

    C = 0;
    auto total_start_time = high_resolution_clock::now();

    for (int i = 0; i < 3 * K; i++)
    {
        thread_data_ptrs[i] = new thread_data(i);
        pthread_create(&threads[i], NULL, (void *(*)(void *))check_sudoku, thread_data_ptrs[i]);
    }

    for (int i = 0; i < 3 * K; i++)
    {
        pthread_join(threads[i], NULL);
        delete thread_data_ptrs[i];
    }

    auto total_end_time = high_resolution_clock::now();
    double total_time = duration_cast<microseconds>(total_end_time - total_start_time).count();

    if (sudoku_valid)
    {
        output_file << "The sudoku is valid.\n";
    }
    else
    {
        output_file << "The sudoku is invalid.\n";
    }
    output_file << "The total time taken is " << total_time << " microseconds.\n";
    output_file << "Average time taken by a thread to enter the CS is " << average(entrytimes) << " microseconds\n";
    output_file << "Average time taken by a thread to exit the CS is " << average(exittimes) << " microseconds\n";
    output_file << "Worst time taken by a thread to enter the CS is " << worstentrytime << " microseconds\n";
    output_file << "Worst time taken by a thread to exit the CS is " << worstexittime << " microseconds\n";

    if (sudoku_valid)
    {
        output_file << "The sudoku is valid.\n";
    }
    else
    {
        output_file << "The sudoku is invalid.\n";
    }
    return 0;
}