#!/bin/bash

mstool="../../../build/mstool"
flesnet="../../../build/flesnet"
tsclient="../../../build/tsclient"
archive_validator="../../../build/archive_validator"

# Checks if the given argument is an integer
# $1 - number to check
function is_number() {
    case $1 in
        ''|*[!0-9]*) echo 0;; # no - is not a number
        *) echo 1 ;; # yes - is number
    esac
}   

# Creates msa files with mstool, feeds them into flesnet via mstool and shm,
# starts entry and build nodes and validates the generated tsa files agains the
# used msa files.
# $1 - number of entry nodes
# $2 - number of build nodes
# $3 - number of timeslices
# $4 - timeslice size
# $5 - overlap size
function test_chain() {
    # Check correct number of arguments
    if [ $# -ne 5 ]; then
        return 1;
    fi
    
    # Check if all arguments are actual integers
    # Strings could do serious harm in later contexts
    for arg in "$@"
    do
        local num_check=$(is_number $arg)
        if [ $num_check -ne 1 ]; then
            return 1
        fi
    done

    local entry_cnt=$1 # how many entry nodes
    local build_cnt=$2 # how many build nodes
    local timeslice_cnt=$3 # how many timeslices do we want to construct
    local timeslice_size=$4 # size of the timeslices
    local overlap_size=$5 # size of overlap between timeslices
    local dir_name="$1to$2_test" # the directory which will contain the generated msa/tsa archive files
    local microslice_cnt=$(($timeslice_size * $timeslice_cnt + $overlap_size))
    rm -rf "./$dir_name" # Cleanup leftovers from last testrun
    mkdir "./$dir_name"
    
    # Use mstool to create input microslice archives
    for i in $(seq 0 $(($entry_cnt - 1)));
    do
        $mstool -p 0 -n $microslice_cnt -c $i -o "./$dir_name/input$i.msa"
        if [ $? -ne 0 ]; then
            return 1;
        fi
    done

    # Use mstool to provide the created microslice archives via shared memory
    local mstool_pids=()
    for i in $(seq 0 $(($entry_cnt - 1)));
    do
        rm -rf /dev/shm/fles_in_shared_memory$i # Cleanup leftovers from last testrun
        $mstool -c $i --input-archive "./$dir_name/input$i.msa" --output-shm fles_in_shared_memory$i > /dev/null 2>&1 &
        mstool_pids+=($!)
    done

    # reference entry node command: ./flesnet -n 15 -t zeromq -i 1 -I shm:/fles_in_shared_memory0/0 shm:/fles_in_shared_memory1/0 -O shm:/fles_out_shared_memory0/0 shm:/fles_out_shared_memory1/0 --processor-instances 1 -e "./tsclient -i shm:%s -o file:timeslice_archive.tsa"
    local entry_input_vector_arg="" # -I arg for entry nodes
    # reference build node command: ./flesnet -n 15 -t zeromq -o 1 -I shm://127.0.0.1/0 shm://127.0.0.1/0 -O shm:/fles_out_shared_memory0/0 shm:/fles_out_shared_memory1/1 --processor-instances 1 -e "./tsclient -i shm:%s -o file:timeslice_archive1.tsa"
    local build_input_vector_arg="" # -I arg for build nodes
    for i in $(seq 0 $(($entry_cnt  - 1)));
    do
        entry_input_vector_arg=" $entry_input_vector_arg shm:/fles_in_shared_memory$i/0?overlap=$overlap_size"
        build_input_vector_arg=" $build_input_vector_arg shm://127.0.0.1/0?overlap=$overlap_size"
    done
    
    local output_vector_arg="" # -O arg for build nodes
    for i in $(seq 0 $(($build_cnt  - 1)));
    do
        output_vector_arg=" $output_vector_arg shm:/fles_out_shared_memory$i/0?overlap=$overlap_size"
    done

    # Start build nodes
    local build_pids=()
    for i in $(seq 0 $(($build_cnt - 1)));
    do
        local processor_executable="$tsclient --input-uri shm:%s -o file:./$dir_name/output$i.tsa"
        $flesnet -n $timeslice_cnt --timeslice-size $timeslice_size -o $i -I $build_input_vector_arg -O $output_vector_arg --processor-instances 1 -e "$processor_executable" -t zeromq > /dev/null 2>&1 &
        build_pids+=($!)
    done
    echo "Started $build_cnt build nodes" 

    # Start entry nodes
    local entry_pids=()
    for i in $(seq 0 $(($entry_cnt - 1)));
    do
        local processor_executable="$tsclient --input-uri shm:%s -o file:./$dir_name/output$i.tsa"
        $flesnet -n $timeslice_cnt --timeslice-size $timeslice_size -i $i -I $entry_input_vector_arg -O $output_vector_arg -t zeromq --processor-instances 1 -e "$processor_executable" > /dev/null 2>&1 &
        entry_pids+=($!)
    done
    echo "Started $entry_cnt entry nodes" 

    echo "Wating for flesnet to finish data processing ..." 

    # Wait for build and entry nodes to exit
    wait ${build_pids[@]}
    wait ${entry_pids[@]}
    for pid in "${mstool_pids[@]}"; # mstool has to be killed forcefully
    do
        kill -9 $pid > /dev/null 2>&1
    done

    local msa_files=$(find ./$dir_name -name "*.msa")
    local tsa_files=$(find ./$dir_name -name "*.tsa")
    # $archive_validator -I ${msa_files[@]} -O ${tsa_files[@]} --timeslice-size $timeslice_size --timeslice-cnt $timeslice_cnt --overlap $overlap_size
    $archive_validator -I ${msa_files[@]} -O ${tsa_files[@]} --timeslice-size $timeslice_size --overlap $overlap_size
    return $?
}

test_chain 1 1 15 100 1
if [ $? -ne 0 ]; then
    echo -e "\e[1;37;1;41mTests FAILED\e[0m" >&2
    exit 1
fi

test_chain 1 1 15 100 15
if [ $? -ne 0 ]; then
    echo -e "\e[1;37;1;41mTests FAILED\e[0m" >&2
    exit 1
fi      

test_chain 1 1 15 200 1
if [ $? -ne 0 ]; then
    echo -e "\e[1;37;1;41mTests FAILED\e[0m" >&2
    exit 1
fi

test_chain 1 1 15 200 2
if [ $? -ne 0 ]; then
    echo -e "\e[1;37;1;41mTests FAILED\e[0m" >&2
    exit 1
fi

test_chain 3 2 15 100 1
if [ $? -ne 0 ]; then
    echo -e "\e[1;37;1;41mTests FAILED\e[0m" >&2
    exit 1
fi

test_chain 3 2 15 200 1
if [ $? -ne 0 ]; then
    echo -e "\e[1;37;1;41mTests FAILED\e[0m" >&2
    exit 1
fi

test_chain 3 2 80 200 1
if [ $? -ne 0 ]; then
    echo -e "\e[1;37;1;41mTests FAILED\e[0m" >&2
    exit 1
fi


test_chain 6 3 80 200 1
if [ $? -ne 0 ]; then
    echo -e "\e[1;37;1;41mTests FAILED\e[0m" >&2
    exit 1
fi

test_chain 6 3 80 200 10
if [ $? -ne 0 ]; then
    echo -e "\e[1;37;1;41mTests FAILED\e[0m" >&2
    exit 1
fi

echo -e "\e[1;37;1;42mTests successful\e[0m"
exit 0