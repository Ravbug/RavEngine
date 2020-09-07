#pragma once
#include <unordered_map>
#include "SharedObject.hpp"
#include <typeindex>
#include <unordered_set>
#include "Component.hpp"
#include <functional>

//macro for checking type in template
#define C_REF_CHECK static_assert(std::is_base_of<RavEngine::Component, T>::value, "Template parameter must be a Component subclass");

namespace RavEngine{
	class ComponentStore {
	protected:
		typedef std::unordered_map<std::type_index, std::list<Ref<RavEngine::Component>>> ComponentStructure;
		ComponentStructure components;
		ComponentStructure componentsRedundant;

	public:
		/**
		Remove all components from this store
		*/
		void clear() {
			components.clear();
			componentsRedundant.clear();
		}

		/**
		 Add a component to this Entity
		 @param componentRef the component to add to this Entity. Must pass a Ref class to a Component
		 */
		template<typename T>
		Ref<T> AddComponent(Ref<T> componentRef) {
			C_REF_CHECK
				components[std::type_index(typeid(T))].push_back(componentRef);

			//add redundant types
			for (const auto& alt : componentRef->getAltTypes()) {
				componentsRedundant[alt].push_back(componentRef);
			}
			return componentRef;
		}

		/**
		 Get the first component of a type  in this Entity. Use as GetComponent<ComponentRef, Component>(). Investigates base classes.
		 @throws runtime_error if no component of type is found on this Entity. If you are not sure, call HasComponentOfType() first
		 @throws bad_cast if a cast fails
		 */
		template<typename T>
		Ref<T> GetComponent() {
			C_REF_CHECK
				auto& vec = components[std::type_index(typeid(T))];
			if (vec.empty()) {
				//try base classes
				return GetComponentOfSubclass<T>();
			}
			else {
				return vec.front();;
			}
		}

		/**
		 Determines if this Entity has a component of a type. Pass in the ref type. Investigates only base classes the passed ref type.
		 @return true if this entity has a component of type ref
		 */
		template<typename T>
		bool HasComponentOfType() {
			C_REF_CHECK
			return (!components[std::type_index(typeid(T))].empty()) || (HasComponentOfSubclass<T>());
		}

		/**
		 Determines if this Entity has a component of a base type. Only considers base classes of ref. Pass in the ref type.
		 @return true if this entity has a component of type ref
		 */
		template<typename T>
		bool HasComponentOfSubclass() {
			C_REF_CHECK
			return !componentsRedundant[std::type_index(typeid(T))].empty();
		}

		/**
		 * Gets the first stored reference of a subclass type. Only use if you know the component you're querying is unique
		 @return the component of the base class if found
		 @throws runtime_error if no component of type is found on this Entity. If you are not sure, call HasComponentOfType() first
		 @throws bad_cast if a cast fails
		 */
		template<typename T>
		Ref<T> GetComponentOfSubclass() {
			C_REF_CHECK
				auto& vec = componentsRedundant[std::type_index(typeid(T))];
			if (vec.size() == 0) {
				throw std::runtime_error("No component of type");
			}
			else {
				return vec.front();
			}
		}

		/**
		For internal use only.
		@param type the type_index to query for
		@return all the components of a class or its base classes to a type index
		*/
		template<typename T>
		std::list<Ref<T>> GetAllComponentsOfSubclassTypeIndex(const std::type_index& type) {
			C_REF_CHECK
			//query both types
			auto& toplevel = components[type];
			auto& comp = componentsRedundant[type];

			//insert into
			std::list<Ref<T>> cpy;
			cpy.insert(cpy.begin(), toplevel.begin(), toplevel.end());
			cpy.insert(cpy.end(), comp.begin(), comp.end());
			return cpy;

		}

		/**
		For internal use only.
		@param type the type_index to query for.
		@return all the components of a type index. Does NOT search base classes
		*/
		template<typename T>
		std::list<Ref<T>> GetAllComponentsOfTypeIndex(const std::type_index& index) {
			C_REF_CHECK
				auto& comp = components[index];
			std::list<Ref<T>> cpy;
			for (auto& c : comp) {
				cpy.push_back(c);
			}
			return cpy;
		}

		/**
		 Gets all of the references of a base class type.
		 @return the list of all the refs of a base class type. The list may be empty.
		 */
		template<typename T>
		std::list<Ref<T>> GetAllComponentsOfSubclass() {
			return GetAllComponentsOfSubclassTypeIndex<T>(std::type_index(typeid(T)));
		}

		/**
		 Get all of the components of a specific type. Does NOT search subclasses.
		 Pass the ref type and the type.
		 @returns the list of only the components of the high-level type. The list may be empty
		 */
		template<typename T>
		std::list<Ref<T>> GetAllComponentsOfType() {
			return GetAllComponentsOfTypeIndex<T>(std::type_index(typeid(T)));
		}

		/**
		Remove a component by value
		@param component the component to remove
		*/
		template<typename T>
		void RemoveComponent(Ref<T> component) {
			components[std::type_index(typeid(T))].remove(component);

			//add redundant types
			for (const auto& alt : component->getAltTypes()) {
				componentsRedundant[alt].remove(component);
			}
		}

		/**
		Copy components from one componentstore into this one
		@param other the other component store to copy from
		*/
		void AddComponentsFrom(const ComponentStore& other) {
			for (const auto& c : other.components) {
				//add the componets of the current type to the global list
				components[c.first].insert(components[c.first].begin(), c.second.begin(), c.second.end());
			}
			//add to the redundant store
			for (const auto& c : other.componentsRedundant) {
				componentsRedundant[c.first].insert(componentsRedundant[c.first].begin(), c.second.begin(), c.second.end());
			}
		}

		/**
		Remove components from this store if they are also present in another
		@param other the ComponentStore to compare
		*/
		void RemoveComponentsInOtherFromThis(const ComponentStore& other) {
			//remove if the components are equal

			auto check = [](ComponentStructure& removeFrom, const ComponentStructure& otherStructure) {
				for (const auto& typepair : otherStructure) {
					//go over elements in template
					for (const auto& candidate : typepair.second) {
						//if the target has this type
						if (removeFrom.find(typepair.first) != removeFrom.end()) {
							removeFrom[typepair.first].remove_if([&](const Ref<RavEngine::Component>& com) {
								return com == candidate;
								});
						}
					}
				}
			};
			check(components, other.components);
			check(componentsRedundant, other.componentsRedundant);
		}
	};
}