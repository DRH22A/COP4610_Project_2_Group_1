#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/string.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/kthread.h>
#include <linux/mutex.h>
#include <linux/delay.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Group 1");
MODULE_DESCRIPTION("Pet Elevator Kernel Module");

// Proc file variables
#define PROC_NAME "elevator"
#define NUM_FLOORS 5
#define MAX_CAPACITY 5
#define MAX_WEIGHT 50

// Pet types
#define PET_CHIHUAHUA 0
#define PET_PUG 1
#define PET_PUGHUAHUA 2
#define PET_DACHSHUND 3

// Pet weights
static const int pet_weights[] = {3, 14, 10, 16};
static const char *pet_names[] = {"Chihuahua", "Pug", "Pughuahua", "Dachshund"};

// Pet structure
typedef struct {
    int type;
    int start_floor;
    int destination_floor;
    int weight;
    struct list_head list;
} Pet;

// Floor structure
typedef struct {
    int num_waiting;
    int waiting_weight;
    struct list_head waiting_pets;
} Floor;

// Elevator states
typedef enum {
    OFFLINE,
    IDLE,
    LOADING,
    UP,
    DOWN
} ElevatorState;

// Elevator structure
typedef struct {
    ElevatorState state;
    int current_floor;
    int num_pets;
    int current_weight;
    struct list_head pets_on_elevator;
    bool should_stop;
} Elevator;

// Globals
static Elevator elevator;
static Floor floors[NUM_FLOORS];
static struct mutex elevator_mutex;
static struct task_struct *elevator_thread;
static struct proc_dir_entry *proc_entry;

// Stats
static int total_pets_serviced = 0;
static int total_pets_waiting = 0;

// Helpers
static const char *get_state_string(ElevatorState state) {
    switch (state) {
        case OFFLINE: return "OFFLINE";
        case IDLE: return "IDLE";
        case LOADING: return "LOADING";
        case UP: return "UP";
        case DOWN: return "DOWN";
        default: return "UNKNOWN";
    }
}

// Pet's beginning character
static char get_pet_char(int type) {
    switch (type) {
        case PET_CHIHUAHUA: return 'C';
        case PET_PUG: return 'P';
        case PET_PUGHUAHUA: return 'H';
        case PET_DACHSHUND: return 'D';
        default: return '?';
    }
}

// Logic for if a pet can board the elevator
static bool can_board_pet(Pet *pet) {
    return (elevator.num_pets < MAX_CAPACITY) &&
           (elevator.current_weight + pet->weight <= MAX_WEIGHT);
}

// Adds a pet to a floor
static int add_pet_to_floor(int floor, Pet *pet) {
    list_add_tail(&pet->list, &floors[floor].waiting_pets);
    floors[floor].num_waiting++;
    floors[floor].waiting_weight += pet->weight;
    total_pets_waiting++;
    return 0;
}

// Loads pets up
static void load_pets(void) {
    Pet *pet, *tmp;
    int floor_index = elevator.current_floor - 1;
    list_for_each_entry_safe(pet, tmp, &floors[floor_index].waiting_pets, list) {
        if (pet->destination_floor == elevator.current_floor) continue;
        if (can_board_pet(pet)) {
            list_del(&pet->list);
            floors[floor_index].num_waiting--;
            floors[floor_index].waiting_weight -= pet->weight;
            total_pets_waiting--;

            list_add_tail(&pet->list, &elevator.pets_on_elevator);
            elevator.num_pets++;
            elevator.current_weight += pet->weight;
        } else break;
    }
}

// Unloads pets
static void unload_pets(void) {
    Pet *pet, *tmp;
    list_for_each_entry_safe(pet, tmp, &elevator.pets_on_elevator, list) {
        if (pet->destination_floor == elevator.current_floor) {
            list_del(&pet->list);
            elevator.num_pets--;
            elevator.current_weight -= pet->weight;
            total_pets_serviced++;
            kfree(pet);
        }
    }
}

// Check the pets waiting above
static bool pets_waiting_above(void) {
    int i;
    for (i = elevator.current_floor; i < NUM_FLOORS; i++)
        if (floors[i].num_waiting > 0) return true;
    return false;
}

// Check the pets waiting below
static bool pets_waiting_below(void) {
    int i;
    for (i = 0; i < elevator.current_floor - 1; i++)
        if (floors[i].num_waiting > 0) return true;
    return false;
}

// Check the pets going up
static bool pets_going_up(void) {
    Pet *pet;
    list_for_each_entry(pet, &elevator.pets_on_elevator, list)
        if (pet->destination_floor > elevator.current_floor) return true;
    return false;
}

// Check the pets going down
static bool pets_going_down(void) {
    Pet *pet;
    list_for_each_entry(pet, &elevator.pets_on_elevator, list)
        if (pet->destination_floor < elevator.current_floor) return true;
    return false;
}

// Determine next state
static ElevatorState determine_next_direction(void) {
    if (elevator.should_stop && elevator.num_pets == 0 && total_pets_waiting == 0)
        return OFFLINE;
    if (elevator.num_pets == 0 && total_pets_waiting == 0)
        return IDLE;
    if (elevator.state == UP && (pets_going_up() || pets_waiting_above()))
        return UP;
    if (elevator.state == DOWN && (pets_going_down() || pets_waiting_below()))
        return DOWN;
    if (pets_going_up() || pets_waiting_above()) return UP;
    if (pets_going_down() || pets_waiting_below()) return DOWN;
    return IDLE;
}

// Elevator thread
static int elevator_run(void *data) {
    while (!kthread_should_stop()) {
        mutex_lock(&elevator_mutex);
        if (elevator.state == OFFLINE) {
            mutex_unlock(&elevator_mutex);
            ssleep(1);
            continue;
        }

        elevator.state = LOADING;
        unload_pets();
        load_pets();
        mutex_unlock(&elevator_mutex);
        ssleep(1);

        mutex_lock(&elevator_mutex);
        elevator.state = determine_next_direction();

        if (elevator.state == UP && elevator.current_floor < NUM_FLOORS) {
            mutex_unlock(&elevator_mutex);
            ssleep(2);
            mutex_lock(&elevator_mutex);
            elevator.current_floor++;
        } else if (elevator.state == DOWN && elevator.current_floor > 1) {
            mutex_unlock(&elevator_mutex);
            ssleep(2);
            mutex_lock(&elevator_mutex);
            elevator.current_floor--;
        }

        mutex_unlock(&elevator_mutex);
        msleep(100);
    }
    return 0;
}

// Syscalls
int start_elevator_syscall(void) {
    mutex_lock(&elevator_mutex);
    if (elevator.state != OFFLINE) { mutex_unlock(&elevator_mutex); return 1; }
    elevator.state = IDLE;
    elevator.current_floor = 1;
    elevator.num_pets = 0;
    elevator.current_weight = 0;
    elevator.should_stop = false;
    mutex_unlock(&elevator_mutex);
    printk(KERN_INFO "elevator: started\n");
    return 0;
}
EXPORT_SYMBOL(start_elevator_syscall);

int issue_request_syscall(int start_floor, int dest_floor, int type) {
    Pet *pet;
    if (start_floor < 1 || start_floor > NUM_FLOORS ||
        dest_floor < 1 || dest_floor > NUM_FLOORS ||
        type < 0 || type > 3 ||
        start_floor == dest_floor) return 1;

    pet = kmalloc(sizeof(Pet), GFP_KERNEL);
    if (!pet) return -ENOMEM;

    pet->type = type;
    pet->start_floor = start_floor;
    pet->destination_floor = dest_floor;
    pet->weight = pet_weights[type];
    INIT_LIST_HEAD(&pet->list);

    mutex_lock(&elevator_mutex);
    add_pet_to_floor(start_floor - 1, pet);
    mutex_unlock(&elevator_mutex);

    printk(KERN_INFO "elevator: %s added to floor %d -> %d\n",
           pet_names[type], start_floor, dest_floor);
    return 0;
}
EXPORT_SYMBOL(issue_request_syscall);

int stop_elevator_syscall(void) {
    mutex_lock(&elevator_mutex);
    if (elevator.should_stop || elevator.state == OFFLINE) { mutex_unlock(&elevator_mutex); return 1; }
    elevator.should_stop = true;
    mutex_unlock(&elevator_mutex);
    printk(KERN_INFO "elevator: stop requested\n");
    return 0;
}
EXPORT_SYMBOL(stop_elevator_syscall);

// Proc file
static int elevator_proc_show(struct seq_file *m, void *v) {
    int i;
    Pet *pet;
    mutex_lock(&elevator_mutex);

    seq_printf(m, "Elevator state: %s\n", get_state_string(elevator.state));
    seq_printf(m, "Current floor: %d\n", elevator.current_floor);
    seq_printf(m, "Current load: %d lbs\n", elevator.current_weight);

    seq_printf(m, "Elevator status: ");
    if (list_empty(&elevator.pets_on_elevator)) {
        seq_puts(m, "empty");
    } else {
        list_for_each_entry(pet, &elevator.pets_on_elevator, list)
            seq_printf(m, "%c%d ", get_pet_char(pet->type), pet->destination_floor);
    }
    seq_puts(m, "\n");

    for (i = NUM_FLOORS-1; i >= 0; i--) {
        seq_printf(m, "[%c] Floor %d: %d ", (elevator.current_floor == i+1?'*':' '), i+1, floors[i].num_waiting);
        list_for_each_entry(pet, &floors[i].waiting_pets, list)
            seq_printf(m, "%c%d ", get_pet_char(pet->type), pet->destination_floor);
        seq_puts(m, "\n");
    }

    seq_printf(m, "Number of pets: %d\n", elevator.num_pets);
    seq_printf(m, "Number of pets waiting: %d\n", total_pets_waiting);
    seq_printf(m, "Number of pets serviced: %d\n", total_pets_serviced);
    mutex_unlock(&elevator_mutex);
    return 0;
}

static int elevator_proc_open(struct inode *inode, struct file *file) {
    return single_open(file, elevator_proc_show, NULL);
}

static const struct proc_ops elevator_proc_fops = {
    .proc_open = elevator_proc_open,
    .proc_read = seq_read,
    .proc_lseek = seq_lseek,
    .proc_release = single_release,
};

// Module init/exit
static int __init elevator_init(void) {
    int i;
    printk(KERN_INFO "elevator: init\n");

    mutex_init(&elevator_mutex);

    elevator.state = OFFLINE;
    elevator.current_floor = 1;
    elevator.num_pets = 0;
    elevator.current_weight = 0;
    elevator.should_stop = false;
    INIT_LIST_HEAD(&elevator.pets_on_elevator);

    for (i = 0;i < NUM_FLOORS;i++) {
        INIT_LIST_HEAD(&floors[i].waiting_pets);
        floors[i].num_waiting = 0;
        floors[i].waiting_weight = 0;
    }

    proc_entry = proc_create(PROC_NAME, 0444, NULL, &elevator_proc_fops);
    if (!proc_entry) return -ENOMEM;

    elevator_thread = kthread_run(elevator_run, NULL, "elevator_thread");
    if (IS_ERR(elevator_thread)) { remove_proc_entry(PROC_NAME,NULL); return PTR_ERR(elevator_thread); }

    return 0;
}

static void __exit elevator_exit(void) {
    int i;
    Pet *pet, *tmp;

    if (elevator_thread) kthread_stop(elevator_thread);
    remove_proc_entry(PROC_NAME,NULL);

    mutex_lock(&elevator_mutex);
    list_for_each_entry_safe(pet,tmp,&elevator.pets_on_elevator,list){ list_del(&pet->list); kfree(pet);}
    for(i =0 ;i < NUM_FLOORS;i++)
        list_for_each_entry_safe(pet,tmp,&floors[i].waiting_pets,list){ list_del(&pet->list); kfree(pet);}
    mutex_unlock(&elevator_mutex);
}

module_init(elevator_init);
module_exit(elevator_exit);
