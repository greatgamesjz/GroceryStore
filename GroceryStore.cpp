#include <iostream>
#include <queue>
#include <vector>
#include <thread>
#include <condition_variable>
#include <mutex>
#include <functional>
#include <memory>
#define test

using namespace std;
struct order
{
	char name_;
public:
	order(char name)
	{
		name_ = name;
	}
	char getName()
	{
		return name_;
	}
};
struct fruit
{
	char name_;
public:
	fruit(char name)
	{
		name_ = name;
	}
	char getName()
	{
		return name_;
	}
};

class shop
{
	typedef std::chrono::duration<int> seconds_type;

	mutex steeringMutex; //steering 3rd thread
	mutex orderListMutex; //preventing 3rd thread from modifying orderList when new 2nd thread is adding new order
	mutex orderListLoadMutex; //preventing 2nd thread from adding new order when 3rd thread is modyfying orderList
	condition_variable steeringCon_var;
	condition_variable orderListCon_var;
	condition_variable orderListLoadCon_var;

	string income_; 
	string orders_;

	vector<fruit*>* shopCollection_; //items in shop
	vector<order*>* orderList_; //ordered items

	fruit* newFruit;
	order* newOrder;

	int time_; //time between orders
	
	int orderListIterator;
	int incomeListIterator;

	bool threadOn;

	bool m_bNewIncomeIsReady; //true when 3rd thread should start realizing new order
	bool m_bIncomeIsAvailable; //true when new income will come
	bool m_bIsOrderListSafe; //true when thread are not using orderList
	
public:
	shop(string income,string orders,int time, vector<fruit*>* shopCollection, vector<order*>* orderList)
	{
		orderListIterator = 0;
		incomeListIterator = 0;
		
		m_bIncomeIsAvailable = true;
		m_bIsOrderListSafe = true;
		shopCollection_ = shopCollection;
		orderList_ = orderList;
		time_ = time;
		orders_ = orders;
		income_ = income;
		threadOn = true;
		m_bNewIncomeIsReady = false;
	}
	~shop()
	{
		delete newFruit;
		delete newOrder;
	}
	void loadProducts()
	{
		seconds_type timeBetweenAddingNewItemToShop(60/10);
		chrono::seconds delay(timeBetweenAddingNewItemToShop); // 10 orders per minute
		while (incomeListIterator < (int)income_.length()) // reading whole income string
		{
			this_thread::sleep_for(delay); 
			m_bNewIncomeIsReady = false; //sleeping 3rd thread
#ifdef test
			cout << "Added product: " << income_[incomeListIterator] << endl;
#endif
			
			newFruit = new fruit(income_[incomeListIterator]); 
			shopCollection_->push_back(newFruit); //pushing new fruit to shop collection
			if (incomeListIterator == (int)income_.length() - 1) //informing 3rd thread about end of income
				stopRealising();
			incomeListIterator += 2;

			m_bNewIncomeIsReady = true; 
			steeringCon_var.notify_one(); //waking up 3rd thread
		}
		
	}
	void loadOrderList()
	{
		unique_lock<mutex> orderListLoadLock(orderListLoadMutex); //preventing 2nd thread from adding new order when 3rd thread is modyfying orderList
		
		seconds_type timeBetweenOrders(time_); //converting int time between new orders from stdin to seconds_type
		while (orderListIterator < (int)orders_.length()) //reading all orders
		{
			newOrder= new order((char)orders_[orderListIterator]);
#ifdef test
			cout << "Added Order: " << newOrder->getName() << endl;
#endif
			orderListLoadCon_var.wait(orderListLoadLock, std::bind(&shop::isOrderListSafe, this)); //checking if orderList is not in use
			lockOrderList(); 
			addOrder(newOrder); //adding new order to orderList
			unlockOrderList();
			orderListCon_var.notify_one(); //letting 3rd thread to modify orderList

			orderListIterator += 2;
			this_thread::sleep_for(timeBetweenOrders);
		}
	}
	void realiseOrder()
	{
		unique_lock<mutex> steeringLock(steeringMutex); 
		unique_lock<mutex> orderListLock(orderListMutex);
		while (incomeIsAvailable())
		{
			steeringCon_var.wait(steeringLock, std::bind(&shop::newIncomeIsReady, this)); //waiting until 1st thread add new item to shop collection
			orderListCon_var.wait(orderListLock, std::bind(&shop::isOrderListSafe, this)); //waiting until 2nd thread stop using orderList
			lockOrderList(); //prevetning 2nd thread from adding new item to orderList 
			for (int i = 0; i < orderList_->size(); i++) //looking for oldest order 
			{
				if (orderList_->at(i)->getName() == shopCollection_->back()->getName()) //checking names of ruits
				{
#ifdef test
					cout << "ERASE: " << orderList_->at(i)->getName() << endl;
#endif
					orderList_->erase(orderList_->begin() + i); //erasing order from orderList
					shopCollection_->pop_back(); //deleting fruit form shop Collection
					break;
				}
			}
			unlockOrderList(); 
			orderListLoadCon_var.notify_one();
			realisingComplete();
		}
	}
	void addOrder(order* newOrder)
	{
		orderList_->push_back(newOrder);
	}
	bool newIncomeIsReady()
	{
		return m_bNewIncomeIsReady;
	}
	bool incomeIsAvailable()
	{
		return m_bIncomeIsAvailable;
	}
	bool isOrderListSafe()
	{
		return m_bIsOrderListSafe;
	}
	void realisingComplete()
	{
		m_bNewIncomeIsReady = false;
	}
	void stopRealising()
	{
		m_bIncomeIsAvailable = false;
	}
	void unlockOrderList()
	{
		m_bIsOrderListSafe = true;
	}
	void lockOrderList()
	{
		m_bIsOrderListSafe = false;
	}
};
int main()
{
	int timeBetweenOrdersINT;
	string income, orders;


	unique_ptr<shop> shop_unqptr;
	unique_ptr<vector<order*>> orderList_unqptr;
	unique_ptr<vector<fruit*>> shopCollection_unqptr;

	cin >> timeBetweenOrdersINT >> income >> orders;


	/*create objects*/
	orderList_unqptr.reset(new vector<order*>);
	shopCollection_unqptr.reset(new vector<fruit*>);
	shop_unqptr.reset(new shop(income,orders, timeBetweenOrdersINT,shopCollection_unqptr.get(),orderList_unqptr.get()));

	/*start new threads*/
	thread shopWorkerThread(&shop::loadProducts,shop_unqptr.get());
	thread orderRealisatorThread(&shop::realiseOrder, shop_unqptr.get());
	thread incomingOrdersThread(&shop::loadOrderList,shop_unqptr.get());







	shopWorkerThread.join();
	incomingOrdersThread.join();
	orderRealisatorThread.join();

	
	
	cout << orderList_unqptr->size() << endl;

	shop_unqptr.reset();
	orderList_unqptr.reset();
	shopCollection_unqptr.reset();


	return 0;
}
